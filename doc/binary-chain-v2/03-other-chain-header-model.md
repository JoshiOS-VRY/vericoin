# Binary Chain v2 — Other-Chain Header Model

> Consensus / node layer (layer 1 + 2). Storage is node-local; the validation rules
> that gate anchor activation are consensus. Builds on DACE-6 (P2P) and DACE-7 (IBD).

## 1. Existing primitive: `PairedHeaderStore`

DACE already ships `dace::PairedHeaderStore` (`paired_view.{h,cpp}`) implementing
`IPairedChainView`:

- `IngestHeaders(vector<CBlockHeader>)` — builds in-memory `CBlockIndex` from received
  headers, keyed by hash + a height map.
- `TipHeight()`, `AtHeight(h)` — query interface used by beacon selection and activation.
- `TrimAbove(h)` — prunes the active-chain height mapping on reorg.

There is **no `OtherChainHeaderDB` and no persistence yet** — it is a purely in-memory
P2P cache. v2 must add persistence; this doc specifies the model.

## 2. `OtherChainHeaderDB` (persisted)

Promote `PairedHeaderStore` to a persisted store, matching DACE-7's intent (LevelDB
subdb `xheaders/`, keyed by `chain_id || height || hash`, ~52 MB/year combined).

```text
OtherChainHeaderDB
  key:   chain_id (1 byte) || height (4 bytes BE) || block_hash (32 bytes)
  value: serialized CBlockHeader (legacy 80B below activation, extended 180B at/above)
        + cumulative_work (uint256, VRM) or cumulative_cert_weight (VRC, see §6)

  index: best_header_by_chain[chain_id] -> (height, hash, cumulative_metric)
```

`chain_id`: `0x76` VeriCoin, `0x77` Verium (matches `ChainTag` / the DACE-6 chain-id
prefix). On VeriCoin the "other chain" is Verium and vice versa once `verium/src/dace/`
exists.

## 3. `AcceptOtherHeader` rules

```text
AcceptOtherHeader(chain_id, header):
  1. Syntactic: correct size for the activation regime; fields parse.
  2. Link: header.hashPrevBlock is known in OtherChainHeaderDB
           OR equals the chain's bootstrap header (DACE_BOOTSTRAP_*_HEADER).
  3. PoW check (Verium headers): header meets claimed nBits target;
           recompute scrypt^2 work; reject if target/work inconsistent.
  4. PoST marker check (VeriCoin headers): nVersion bit 30 consistent;
           DO NOT attempt to validate stake kernel from header alone (see §6).
  5. Min-work / min-depth gating for *use*: a header may be stored once linked,
           but may only be USED for beacon selection / activation when buried
           (Verium: K_BEACON deep; see §5).
  6. Bounded ahead: refuse to accept headers more than CoupleLookaheadEpochs (5)
           epochs ahead of the local couplable frontier (anti-flooding).
  7. Misbehavior scoring on violation (DACE-6): invalid xheader = 50.
```

**Invalid / malformed headers are rejected** (and scored). **Valid-but-stale** headers
are stored and simply do not advance the couplable frontier — they degrade Binary Chain
benefits (doc 02), they do not halt either chain.

## 4. Header gossip messages (DACE-6, existing wire format)

| Message | Direction | Payload |
|---------|-----------|---------|
| `getxheaders` | request | `chain_id`, `block_locator[]`, `hash_stop` |
| `xheaders` | response | `chain_id`, `headers[]` |
| `getja` | request | `ja_hash` |
| `ja` | response | full `JointAnchor` |
| `jasig` | gossip | `epoch_index`, `ja_partial_hash`, `committee_index`, `sig` |
| `getclaim`/`claim` | optional | wallet convenience lookups |

**Implementation gap:** `xheaders` ingestion is wired, but `getxheaders` is a no-op
("ack without serving"), and `getja`/`jasig` are no-ops. v2 must implement serving and
the `jasig` assembly window. See doc 09/10.

## 5. Total work / trust accounting

### Verium (PoW) side — straightforward

Verium headers carry objective accumulated work. The DB tracks `cumulative_work` per
header (sum of `2^256 / (target+1)` per the usual rule). A Verium beacon is only
*usable* when it is `K_BEACON` (50) deep on the **most-work** Verium header chain known
to the node. This is standard SPV-grade reasoning and is safe from headers alone.

```text
best_verium = argmax_{h in DB[0x77]} cumulative_work(h)
beacon_e usable iff height(best_verium) - height(candidate_e) >= K_BEACON
```

### VeriCoin (PoST) side — the hard caveat (§6)

## 6. PoST cumulative-trust caveat (read this carefully)

**VeriCoin trust cannot be validated from headers alone.** PoST security is not objective
accumulated work; a header does not let a remote node verify that a stake kernel was
valid, that the staked coins existed/were unspent, or that the chain represents real
coin-age/stake-time. Treating a "cumulative trust" number on the PoST side as if it were
PoW work would be a serious error.

Consequences and the minimum additional data required:

1. **Do not** define a header-only "cumulative trust" metric for VeriCoin and **do not**
   let Verium nodes pick a "best VeriCoin chain" purely from VeriCoin headers.
2. The VeriCoin side's contribution to a Joint Anchor is the **bonded committee
   certificate**, not a header-derived trust score. The committee quorum signature is the
   authenticated object that a Verium node can check — *if* it can authenticate the
   committee set.
3. **Minimum additional data required** for a Verium node (or any node) to trust a
   VeriCoin certificate without running a full VeriCoin node:
   - the **committee root** (`committee_root` in the JA, = Merkle root of selected
     `ticket_id`s), which is committed in VeriCoin coinbase each block; and
   - a **Merkle proof** that the signing operators' `ticket_id`s are in that root; and
   - the **bonded-stake snapshot** sufficient to know each ticket is a real
     `TicketStakeUnit` lock (i.e. the ticket registry root and a proof of the funding
     output's existence/lock at the registration epoch).
   In v2 Phase 1–3, the simplest safe choice is: **the VeriCoin chain authenticates its
   own committee certificate** (it has full state), and the Verium side consumes the
   *activated* JA hash via coinbase inclusion rather than independently re-deriving
   VeriCoin trust. This keeps Verium from needing VeriCoin's UTXO/stake state.
4. A future compact proof (Phase 4) could let either side verify the certificate with a
   succinct committee+stake proof, but **that is explicitly out of scope for v2** and
   must not be faked with a header-only trust number.

> Bottom line: Verium provides objective work that VeriCoin can check from headers.
> VeriCoin provides a stake-bonded certificate that Verium checks via the committee root
> + coinbase-anchored activation, **not** via a header-only trust metric.

## 7. Eclipse resistance and peer diversity

A node that is eclipsed on the *other* chain's header feed could be fed a stale or
withheld view. Mitigations (layer 2, node policy):

- **Multi-peer foreign-header fetching:** request `getxheaders` from ≥ N distinct peers
  (recommend N ≥ 4) and require agreement on the buried (`K_BEACON`-deep) prefix before
  using a beacon.
- **Peer diversity requirements:** prefer peers across distinct netgroups/ASNs for the
  paired-header feed; do not let a single peer be the sole source of the other chain.
- **Anchored bootstrap:** fresh nodes start from `DACE_BOOTSTRAP_*_HEADER` /
  `DACE_BOOTSTRAP_JA_GENESIS` in chainparams, so an attacker cannot rewrite history before
  the bootstrap point.
- **Stale-coupling, not trust:** if the foreign feed is withheld, the node enters
  stale-coupling (doc 02) and degrades bonus — it never accepts an unburied beacon or an
  unauthenticated certificate to "make progress."
- **Failure is bounded:** the worst case of a foreign-header eclipse is degraded bonus and
  paused finality upgrades, not base-chain halt and not acceptance of invalid anchors.

## 8. Parked / stale reference behavior

- **Parked blocks:** local blocks that reference a not-yet-activated anchor are accepted
  under the prior `JA_active` (they are valid); the newer anchor is cached as
  observed/certified until it activates. No block is rejected for "future" anchor data.
- **Stale references:** a `pairedAnchorRef` pointing at anything other than the current
  `JA_active` is invalid (rejected). A `beaconRef` not matching the deterministic beacon
  for the epoch is invalid.
- **Reorg of the foreign chain above the buried depth** cannot change an already-activated
  beacon (ratchet); a reorg below `K_BEACON` simply means the candidate is not yet usable.

## 9. Required tests

- SPV work accounting matches a full Verium node over ≥100k simulated headers.
- VeriCoin certificate verification never relies on a header-only trust number.
- Eclipse drill: single-peer foreign feed cannot advance the couplable frontier.
- Persistence: `OtherChainHeaderDB` survives restart; IBD replay is byte-identical.
- `getxheaders` serving and `jasig` assembly produce certifiable anchors.
