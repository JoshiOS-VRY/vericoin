# DACE-4: Joint Anchors and Three-Phase Lifecycle

Status: Draft
Type: Standards Track (consensus, hard fork)
Created: 2026-05-24

## Abstract

DACE-4 defines the Joint Anchor (`JA_e`) object, its three-phase lifecycle (observed → certified → activated), and the rule that only **activated** anchors may serve as a block-validity dependency. Joint Anchors require both a VRC bonded-ticket committee signature and a sufficiently buried VRM PoW beacon — the design's "both halves" requirement.

## Motivation

v2 used "Joint Finality Records" (JFRs) with an absolute reorg ceiling. That is unsafe: it leaves no protocol-native recovery path if a JFR is wrong. DACE-4 replaces absolute finality with slashable certification plus delayed activation plus bounded stale-coupling plus a high-threshold recovery rule.

## Specification

### Object layout

```
struct JointAnchor {
    uint32_t  epoch_index;         // e
    uint256   prev_anchor_hash;    // JA_{e-1}, or zero for JA_0
    uint256   beacon_ref;          // beacon_e.hash, per DACE-2
    BeaconDepthProof beacon_proof; // K_BEACON-deep succeeding headers, per DACE-2
    uint256   vrc_checkpoint_hash; // hash of VRC tip at end of epoch e
    uint32_t  vrc_checkpoint_height;
    uint256   reward_root_vrc_prev; // rewardAccumulatorRoot for VRC at epoch e-1
    uint256   reward_root_vrm_prev; // rewardAccumulatorRoot for VRM at epoch e-1
    uint256   committee_root;       // per DACE-3, Merkle root of selected ticket_ids
    QuorumSignature signature;      // aggregated committee signature, q quorum
}
```

`JointAnchor.hash() = SHA256d(serialize(JA))`.

### Three-phase lifecycle

| Phase | Trigger | Validity effect |
|-------|---------|------------------|
| **Observed** | All non-signature fields are locally constructable; `beacon_ref` is known but may not yet be `K_BEACON`-deep | None — informational |
| **Certified** | Aggregated signature exists meeting `q` quorum against `committee_root`; beacon is `K_BEACON`-deep | Cacheable; relayable; **not** a current block-validity condition |
| **Activated** | One full epoch (`BEACON_EPOCH_VRC` blocks) has elapsed since certification, and both VRC and VRM have included the JA hash in coinbase | Becomes `JA_active`; all subsequent blocks must reference its hash via `pairedAnchorRef` |

### Activation rule (concrete)

A certified JA `JA_e` becomes activated at VRC height `h_c` when:

```
ja_e.certified_at_vrc_height + BEACON_EPOCH_VRC <= h_c
AND
ja_e has been included in at least one VRM coinbase commitment whose VRM block
also has BEACON_EPOCH_VRM_CONF confirmations (default = 6).
```

Once `JA_active` updates, subsequent VRC and VRM blocks MUST use the new `JA_e.hash()` as `pairedAnchorRef`. Forks that disagree about a not-yet-activated anchor are still allowed under the prior `JA_active` until activation completes deterministically.

### Inclusion deadline

The first VRC block to mine after `JA_e` becomes certified MUST include `JA_e.hash` in a designated coinbase scriptSig push (`OP_JA <ja_hash>`). The first VRM block to mine after `JA_e` becomes certified MUST similarly include `JA_e.hash`.

Failure to include a known certified JA within `FINALITY_GRACE = 18` blocks invalidates further blocks until inclusion happens. This forces miners and stakers to keep their software updated.

### Signature scheme

Phase 1: list of (committee_index, ECDSA signature) pairs. Easiest to implement; signature size `~M * 72` bytes.

Phase 4 (future): Schnorr aggregate via libsecp256k1 musig2 module. Reduces size to ~64 bytes. Not required for initial activation; tracked in [phase-4-hardening-vrf-vdf.md](phase-4-hardening-vrf-vdf.md).

### Slashing

See [DACE-3](DACE-3-bonded-ticket-committee.md). Two conflicting signatures (different `JA_e` hashes, same epoch) from one operator are slashable evidence.

### Liveness and recovery

If certification stalls for `s` consecutive epochs:
- Both chains continue using `JA_active`. Cross-chain finality stops advancing.
- Local block production, payments, mining, and staking are unaffected.
- Wallets surface a `binarychain_finality_stalled` warning after `S_GRACE`.

If the stall reaches `S_MAX`, a **recovery anchor** may be created by a `RECOVERY_THRESHOLD` (default 80%) bonded-ticket-weight supermajority. The recovery anchor still requires a `K_BEACON`-deep PoW beacon and follows the same activated-only validity rule. Recovery is rare-path, instrumented, and alarmed.

## Mining-surface impact on VRM

**Zero.** Joint Anchor signatures are produced exclusively by the VRC bonded-ticket committee. VRM miners do not sign anchors, do not aggregate signatures, do not verify committee signatures, and do not produce any artifact that becomes part of an anchor beyond the normal VRM blocks they already mine. The signature scheme (ECDSA list or future Schnorr) lives on the VRC side and never enters the scrypt² hot path on VRM.

## Reference implementation

- [vericoin/src/dace/joint_anchor.h](../../src/dace/joint_anchor.h)
- [vericoin/src/dace/joint_anchor.cpp](../../src/dace/joint_anchor.cpp)
- [vericoin/src/dace/anchor_lifecycle.h](../../src/dace/anchor_lifecycle.h)
- [vericoin/src/dace/anchor_lifecycle.cpp](../../src/dace/anchor_lifecycle.cpp)
- [vericoin/src/dace/recovery.h](../../src/dace/recovery.h)
- [vericoin/src/dace/recovery.cpp](../../src/dace/recovery.cpp)

## Test vectors

`vericoin/test/dace/anchor_vectors.json` covers:
- JA_0 (genesis anchor) construction and activation at activation height.
- Happy-path lifecycle: observe → certify → activate.
- Equivocation: two signatures for the same epoch produce valid slash.
- Liveness stall: 3 missed epochs produces warning; `S_MAX` triggers recovery eligibility.
- Recovery anchor: `RECOVERY_THRESHOLD` supermajority produces a valid recovery JA that supersedes the stall.
