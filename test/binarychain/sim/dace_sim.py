"""Core DACE algorithms in Python, mirroring vericoin/src/dace/ exactly.

Hash function used everywhere: SHA256d (double SHA-256) to match the
reference CHashWriter pipeline in the C++ implementation.

This file is intentionally self-contained — no external dependencies beyond
the Python stdlib. It can run anywhere Python 3.8+ runs.
"""

from __future__ import annotations

import enum
import hashlib
import struct
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Hash primitive
# ---------------------------------------------------------------------------


def sha256d(b: bytes) -> bytes:
    """SHA256d: double SHA-256, the consensus hash for Bitcoin-derived chains."""
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()


# ---------------------------------------------------------------------------
# Default consensus parameters (Balanced profile from DACE-0).
# ---------------------------------------------------------------------------


@dataclass
class DaceParams:
    binary_chain_height_vrc: int = 50
    binary_chain_height_vrm: int = 50
    beacon_delta: int = 4              # VRM stride between beacon target heights
    beacon_k: int = 4                  # K_BEACON confirmations
    beacon_fallback_window: int = 4    # bounded fallback ladder
    beacon_epoch_vrc: int = 10         # VRC blocks per coupling epoch
    ticket_stake_unit: int = 100 * 100_000_000  # 100 VRC in satoshi
    ticket_lockup_epochs: int = 3
    ticket_unbond_delay_epochs: int = 1
    committee_size: int = 8
    committee_quorum_num: int = 2
    committee_quorum_den: int = 3
    stale_grace_epochs: int = 2
    stale_max_epochs: int = 6
    recovery_threshold_num: int = 4
    recovery_threshold_den: int = 5
    finality_grace_blocks: int = 6
    divert_sigma_bps_vrm: int = 400
    divert_phi_bps_vrc: int = 1000
    claim_expiry_epochs: int = 256


# ---------------------------------------------------------------------------
# Tickets and committee sortition (DACE-3)
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Ticket:
    stake_outpoint: bytes      # 36 bytes (synthetic for sim)
    operator_pubkey: bytes     # 33 bytes (synthetic for sim)
    registered_epoch: int
    stake_amount: int          # satoshi
    unbond_epoch: int = 0      # 0 = active

    def is_active(self, current_epoch: int) -> bool:
        return self.unbond_epoch == 0 or current_epoch < self.unbond_epoch


def ticket_id(t: Ticket) -> bytes:
    """ticket_id = SHA256d(stake_outpoint || operator_pubkey)."""
    return sha256d(t.stake_outpoint + t.operator_pubkey)


def sortition_seed(prev_ja_hash: bytes, beacon_hash: bytes,
                   vrc_checkpoint_hash: bytes) -> bytes:
    """S_e = SHA256d(prev_ja || beacon || vrc_checkpoint)."""
    return sha256d(prev_ja_hash + beacon_hash + vrc_checkpoint_hash)


def ticket_score(seed: bytes, tid: bytes) -> bytes:
    """score_i = SHA256d(seed || ticket_id)."""
    return sha256d(seed + tid)


def sample_committee(tickets: List[Ticket], seed: bytes, M: int) -> List[Ticket]:
    """Select M tickets with lowest scores, without replacement.
    Returned list is sorted by ticket_id for downstream determinism. """
    if not tickets or M == 0:
        return []
    scored: List[Tuple[bytes, bytes, Ticket]] = []
    for t in tickets:
        tid = ticket_id(t)
        scored.append((ticket_score(seed, tid), tid, t))
    # Sort by score, tiebreak by ticket_id, take first M.
    scored.sort(key=lambda x: (x[0], x[1]))
    selected = [t for (_score, _tid, t) in scored[:M]]
    selected.sort(key=lambda t: ticket_id(t))
    return selected


# ---------------------------------------------------------------------------
# Beacon selection (DACE-2)
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Beacon:
    epoch_index: int
    beacon_hash: bytes
    beacon_height: int
    selection_offset: int


def select_beacon(epoch: int, vrm_chain: Dict[int, bytes],
                  params: DaceParams) -> Optional[Beacon]:
    """Deterministic height-based beacon selection per DACE-2.

    vrm_chain: dict mapping VRM height -> block hash for blocks currently
                on the canonical VRM chain.
    """
    H_e = params.binary_chain_height_vrm + epoch * params.beacon_delta
    tip = max(vrm_chain.keys()) if vrm_chain else -1
    for offset in range(params.beacon_fallback_window + 1):
        target = H_e + offset
        if target > tip:
            return None  # epoch undecided; paired chain too short
        if target not in vrm_chain:
            continue     # gap; try next
        depth = tip - target
        if depth < params.beacon_k:
            continue     # not buried deeply enough
        return Beacon(
            epoch_index=epoch,
            beacon_hash=vrm_chain[target],
            beacon_height=target,
            selection_offset=offset,
        )
    return None


# ---------------------------------------------------------------------------
# Joint Anchor (DACE-4)
# ---------------------------------------------------------------------------


@dataclass
class JointAnchor:
    epoch_index: int
    prev_anchor_hash: bytes
    beacon_ref: bytes
    vrc_checkpoint_hash: bytes
    vrc_checkpoint_height: int
    reward_root_vrc_prev: bytes
    reward_root_vrm_prev: bytes
    committee_root: bytes
    signature_pubkeys: List[bytes] = field(default_factory=list)  # committee members who signed

    def partial_hash(self) -> bytes:
        """Hash of non-signature fields (the signing payload)."""
        buf = b""
        buf += struct.pack("<I", self.epoch_index)
        buf += self.prev_anchor_hash
        buf += self.beacon_ref
        buf += self.vrc_checkpoint_hash
        buf += struct.pack("<I", self.vrc_checkpoint_height)
        buf += self.reward_root_vrc_prev
        buf += self.reward_root_vrm_prev
        buf += self.committee_root
        return sha256d(buf)

    def full_hash(self) -> bytes:
        """Full anchor hash including signers. Used as pairedAnchorRef."""
        buf = self.partial_hash()
        for pk in sorted(self.signature_pubkeys):
            buf += pk
        return sha256d(buf)


class AnchorPhase(enum.Enum):
    OBSERVED = "observed"
    CERTIFIED = "certified"
    ACTIVATED = "activated"


@dataclass
class AnchorEntry:
    anchor: JointAnchor
    phase: AnchorPhase = AnchorPhase.OBSERVED
    certified_at_vrc_height: int = -1


class AnchorLifecycle:
    """observe -> certify -> activate state machine. Matches anchor_lifecycle.cpp."""

    def __init__(self):
        self.entries: Dict[bytes, AnchorEntry] = {}  # keyed by partial_hash
        self.active_partial: bytes = b""

    def active(self) -> Optional[JointAnchor]:
        if not self.active_partial:
            return None
        e = self.entries.get(self.active_partial)
        if not e or e.phase != AnchorPhase.ACTIVATED:
            return None
        return e.anchor

    def observe(self, a: JointAnchor) -> None:
        key = a.partial_hash()
        if key in self.entries:
            self.entries[key].anchor = a
            return
        self.entries[key] = AnchorEntry(anchor=a, phase=AnchorPhase.OBSERVED)

    def certify(self, partial_hash: bytes, committee: List[Ticket],
                params: DaceParams) -> bool:
        e = self.entries.get(partial_hash)
        if e is None:
            return False
        sig_count = len(e.anchor.signature_pubkeys)
        if sig_count * params.committee_quorum_den < len(committee) * params.committee_quorum_num:
            return False
        e.phase = AnchorPhase.CERTIFIED
        return True

    def promote_if_activated(self, current_vrc_height: int,
                             vrm_inclusions: Dict[bytes, int],
                             params: DaceParams) -> Optional[JointAnchor]:
        """Activate certified anchors whose epoch-delay and VRM inclusion are
        both satisfied."""
        promoted: Optional[JointAnchor] = None
        promoted_key = b""
        for key, e in self.entries.items():
            if e.phase != AnchorPhase.CERTIFIED:
                continue
            if e.certified_at_vrc_height < 0:
                e.certified_at_vrc_height = current_vrc_height
            if e.certified_at_vrc_height + params.beacon_epoch_vrc > current_vrc_height:
                continue
            full = e.anchor.full_hash()
            depth = vrm_inclusions.get(full, -1)
            BEACON_EPOCH_VRM_CONF = 6
            if depth < BEACON_EPOCH_VRM_CONF:
                continue
            e.phase = AnchorPhase.ACTIVATED
            if promoted is None or promoted.epoch_index < e.anchor.epoch_index:
                promoted = e.anchor
                promoted_key = key
        if promoted is not None:
            self.active_partial = promoted_key
        return promoted

    def detect_equivocation(self, a: JointAnchor) -> Optional[JointAnchor]:
        """Return any other anchor for the same epoch (slashable). """
        for e in self.entries.values():
            if e.anchor.epoch_index == a.epoch_index and e.anchor.partial_hash() != a.partial_hash():
                return e.anchor
        return None


# ---------------------------------------------------------------------------
# Reward accumulator and claims (DACE-5)
# ---------------------------------------------------------------------------


class ClaimSourceChain(enum.IntEnum):
    VRM = 1
    VRC = 2


class ClaimRecipientKind(enum.IntEnum):
    VRC_TICKET_OPERATOR = 1
    VRM_BEACON_MINER = 2


@dataclass
class ClaimLeaf:
    source_chain: ClaimSourceChain
    source_block: bytes      # 32
    epoch: int
    recipient_kind: ClaimRecipientKind
    recipient_pkh: bytes     # 20
    amount: int              # satoshi

    def serialize(self) -> bytes:
        return (
            struct.pack("<B", int(self.source_chain))
            + self.source_block
            + struct.pack("<I", self.epoch)
            + struct.pack("<B", int(self.recipient_kind))
            + self.recipient_pkh
            + struct.pack("<q", self.amount)
        )

    def leaf_hash(self) -> bytes:
        return sha256d(self.serialize())


def merkle_root(hashes: List[bytes]) -> bytes:
    """Bitcoin-style merkle root with duplicate-last-on-odd."""
    if not hashes:
        return b"\x00" * 32
    level = list(hashes)
    while len(level) > 1:
        if len(level) % 2 == 1:
            level.append(level[-1])
        nxt = []
        for i in range(0, len(level), 2):
            nxt.append(sha256d(level[i] + level[i + 1]))
        level = nxt
    return level[0]


def merkle_branch(hashes: List[bytes], index: int) -> List[bytes]:
    """Produce the inclusion-proof branch for `index` over `hashes`."""
    branch: List[bytes] = []
    level = list(hashes)
    i = index
    while len(level) > 1:
        if len(level) % 2 == 1:
            level.append(level[-1])
        sibling_index = i ^ 1
        branch.append(level[sibling_index])
        nxt = []
        for k in range(0, len(level), 2):
            nxt.append(sha256d(level[k] + level[k + 1]))
        level = nxt
        i //= 2
    return branch


def reconstruct_root_from_branch(leaf_hash: bytes, branch: List[bytes],
                                 index: int) -> bytes:
    h = leaf_hash
    i = index
    for sibling in branch:
        if i % 2 == 0:
            h = sha256d(h + sibling)
        else:
            h = sha256d(sibling + h)
        i //= 2
    return h


class RewardAccumulator:
    def __init__(self, epoch: int):
        self.epoch = epoch
        self.leaves: List[ClaimLeaf] = []

    def append(self, leaf: ClaimLeaf) -> None:
        self.leaves.append(leaf)

    def root(self) -> bytes:
        return merkle_root([l.leaf_hash() for l in self.leaves])

    def build_proof(self, index: int) -> Tuple[ClaimLeaf, List[bytes]]:
        return self.leaves[index], merkle_branch(
            [l.leaf_hash() for l in self.leaves], index
        )


def divert_vrm_subsidy(subsidy: int, params: DaceParams) -> int:
    return (subsidy * params.divert_sigma_bps_vrm) // 10000


def divert_vrc_fees(fees: int, params: DaceParams) -> int:
    return (fees * params.divert_phi_bps_vrc) // 10000


# ---------------------------------------------------------------------------
# Stale-coupling tracker (DACE-7)
# ---------------------------------------------------------------------------


class StaleCouplingTracker:
    def __init__(self):
        self.stall_epochs = 0
        self.reason = ""

    def enter_stale(self, reason: str) -> None:
        self.reason = reason

    def leave_stale(self) -> None:
        self.reason = ""
        self.stall_epochs = 0

    def on_epoch_boundary(self, advanced: bool) -> None:
        if advanced and not self.reason:
            self.stall_epochs = 0
        elif self.reason:
            self.stall_epochs += 1

    def warning_level(self, params: DaceParams) -> str:
        if self.stall_epochs >= params.stale_max_epochs:
            return "RecoveryEligible"
        if self.stall_epochs >= params.stale_grace_epochs:
            return "Warn"
        return "None"


# ---------------------------------------------------------------------------
# Top-level simulator (drives a paired VRM + VRC pair through epochs)
# ---------------------------------------------------------------------------


@dataclass
class SimChain:
    name: str
    tip: int = -1
    headers: Dict[int, bytes] = field(default_factory=dict)

    def append(self, h: bytes) -> int:
        self.tip += 1
        self.headers[self.tip] = h
        return self.tip

    def reorg_to(self, height: int) -> None:
        """Drop everything above `height` from the canonical chain."""
        for h in list(self.headers.keys()):
            if h > height:
                del self.headers[h]
        self.tip = max(self.headers.keys()) if self.headers else -1


class BinaryChainSim:
    """Orchestrator that runs the dual-chain DACE protocol in-process."""

    def __init__(self, params: Optional[DaceParams] = None):
        self.params = params or DaceParams()
        self.vrm = SimChain(name="vrm")
        self.vrc = SimChain(name="vrc")
        self.lifecycle = AnchorLifecycle()
        self.stale = StaleCouplingTracker()
        self.tickets: List[Ticket] = []
        self.accumulators: Dict[int, RewardAccumulator] = {}
        self.vrm_inclusions: Dict[bytes, int] = {}  # ja_full_hash -> depth in VRM
        self.claim_spent: set = set()

    # --- mining / staking helpers --------------------------------------
    def mine_vrm(self, count: int = 1) -> None:
        for _ in range(count):
            h = sha256d(f"vrm-{self.vrm.tip + 1}".encode())
            self.vrm.append(h)

    def stake_vrc(self, count: int = 1) -> None:
        for _ in range(count):
            h = sha256d(f"vrc-{self.vrc.tip + 1}".encode())
            self.vrc.append(h)
            self._update_accumulator(h)
            self._tick_inclusions()
            self._maybe_promote()

    # --- ticket helpers ------------------------------------------------
    def register_ticket(self, stake_outpoint: bytes, operator_pubkey: bytes,
                        epoch: Optional[int] = None) -> Ticket:
        if epoch is None:
            epoch = self.current_epoch_vrc() + 1
        t = Ticket(
            stake_outpoint=stake_outpoint,
            operator_pubkey=operator_pubkey,
            registered_epoch=epoch,
            stake_amount=self.params.ticket_stake_unit,
        )
        self.tickets.append(t)
        return t

    # --- epoch / beacon helpers ----------------------------------------
    def current_epoch_vrc(self) -> int:
        if self.vrc.tip < self.params.binary_chain_height_vrc:
            return 0
        return (self.vrc.tip - self.params.binary_chain_height_vrc) // self.params.beacon_epoch_vrc

    def beacon_for_epoch(self, epoch: int) -> Optional[Beacon]:
        return select_beacon(epoch, self.vrm.headers, self.params)

    def active_committee(self) -> List[Ticket]:
        prev_ja = self.lifecycle.active()
        prev_hash = prev_ja.full_hash() if prev_ja else b"\x00" * 32
        beacon = self.beacon_for_epoch(self.current_epoch_vrc())
        beacon_hash = beacon.beacon_hash if beacon else b"\x00" * 32
        checkpoint = self.vrc.headers.get(self.vrc.tip, b"\x00" * 32)
        seed = sortition_seed(prev_hash, beacon_hash, checkpoint)
        active = [t for t in self.tickets if t.is_active(self.current_epoch_vrc())]
        return sample_committee(active, seed, self.params.committee_size)

    # --- JA construction ----------------------------------------------
    def build_anchor_candidate(self) -> Optional[JointAnchor]:
        epoch = self.current_epoch_vrc()
        beacon = self.beacon_for_epoch(epoch)
        if beacon is None:
            self.stale.enter_stale("no beacon available")
            return None
        prev_ja = self.lifecycle.active()
        prev_hash = prev_ja.full_hash() if prev_ja else b"\x00" * 32
        committee = self.active_committee()
        committee_root = merkle_root([ticket_id(t) for t in committee])
        a = JointAnchor(
            epoch_index=epoch,
            prev_anchor_hash=prev_hash,
            beacon_ref=beacon.beacon_hash,
            vrc_checkpoint_hash=self.vrc.headers.get(self.vrc.tip, b"\x00" * 32),
            vrc_checkpoint_height=self.vrc.tip,
            reward_root_vrc_prev=self._closed_root(epoch - 1, ClaimSourceChain.VRC),
            reward_root_vrm_prev=self._closed_root(epoch - 1, ClaimSourceChain.VRM),
            committee_root=committee_root,
        )
        self.lifecycle.observe(a)
        self.stale.leave_stale()
        return a

    def sign_anchor(self, anchor: JointAnchor, committee: List[Ticket],
                    sign_count: int) -> None:
        """Add `sign_count` committee signatures to the anchor (simulated)."""
        anchor.signature_pubkeys = [t.operator_pubkey for t in committee[:sign_count]]
        # Re-observe so the lifecycle sees the new signature shape.
        self.lifecycle.observe(anchor)
        # Try to certify.
        if self.lifecycle.certify(anchor.partial_hash(), committee, self.params):
            # Simulate VRM inclusion by recording the full hash.
            self.vrm_inclusions[anchor.full_hash()] = 0

    # --- internal helpers ---------------------------------------------
    def _update_accumulator(self, block_hash: bytes) -> None:
        epoch = self.current_epoch_vrc()
        acc = self.accumulators.setdefault(epoch, RewardAccumulator(epoch))
        # Synthetic leaf: each VRC block diverts phi of "fees" (1000 sat).
        leaf = ClaimLeaf(
            source_chain=ClaimSourceChain.VRC,
            source_block=block_hash,
            epoch=epoch,
            recipient_kind=ClaimRecipientKind.VRM_BEACON_MINER,
            recipient_pkh=b"\x00" * 20,
            amount=divert_vrc_fees(1000, self.params),
        )
        acc.append(leaf)

    def _closed_root(self, epoch: int, chain: ClaimSourceChain) -> bytes:
        if epoch < 0:
            return b"\x00" * 32
        acc = self.accumulators.get(epoch)
        if acc is None:
            return b"\x00" * 32
        return acc.root()

    def _tick_inclusions(self) -> None:
        """Simulate VRM coinbase inclusion depth growing as VRM blocks come in."""
        for k in self.vrm_inclusions:
            self.vrm_inclusions[k] += 1

    def _maybe_promote(self) -> Optional[JointAnchor]:
        return self.lifecycle.promote_if_activated(
            current_vrc_height=self.vrc.tip,
            vrm_inclusions=self.vrm_inclusions,
            params=self.params,
        )
