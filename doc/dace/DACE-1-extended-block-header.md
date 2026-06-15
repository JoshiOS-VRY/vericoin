# DACE-1: Extended Block Header

Status: Draft
Type: Standards Track (consensus, hard fork)
Created: 2026-05-24

## Abstract

DACE-1 extends `CBlockHeader` on both VRC and VRM with three independent cross-chain fields and an epoch counter. The PoS marker (`nFlags`) is folded into `nVersion`. The change activates at coordinated heights `BinaryChainHeight_VRC` and `BinaryChainHeight_VRM`.

## Motivation

v2 carried cross-chain data in coinbase scriptSig (soft-fork) and earlier proposals used a single omnibus `hashCrossChain` field. Both designs hide ambiguity. DACE separates **observation**, **certification**, and **activation** by giving each its own header field.

## Specification

### Extended header layout (above activation, both chains)

```
struct CBlockHeader_v3 {              // 152 bytes serialized
    int32_t  nVersion;                // bits[0..15] base; bits[16..29] BIP9; bit[30] = PoS marker (VRC only)
    uint256  hashPrevBlock;
    uint256  hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
    uint256  pairedAnchorRef;         // hash of JA_active (latest activated Joint Anchor)
    uint256  beaconRef;               // hash of this epoch's selected paired-chain beacon block
    uint256  rewardAccumulatorRoot;   // Merkle root of cross-chain reward claims, prior epoch
    uint32_t epochIndex;              // current coupling epoch e
};
```

Total serialized size: `4 + 32 + 32 + 4 + 4 + 4 + 32 + 32 + 32 + 4 = 180` bytes.

Note: previous estimates in the design guide quoted 116/156 bytes; the precise figure given the field set above is **180** bytes. Implementations must use this figure for size accounting.

### Field semantics

| Field | Meaning |
|-------|---------|
| `pairedAnchorRef` | The hash of `JA_active` — the latest Joint Anchor that has been *activated*. Newer observed or certified anchors MAY accompany the block as auxiliary objects but MUST NOT appear here. |
| `beaconRef` | The hash of the paired-chain beacon block selected for this epoch per [DACE-2](DACE-2-deterministic-beacon.md). |
| `rewardAccumulatorRoot` | The Merkle root of cross-chain reward claim deltas accumulated locally in the previous epoch. Defined in [DACE-5](DACE-5-delayed-reward-accumulators.md). |
| `epochIndex` | Monotonically increases by 1 each new coupling epoch. Used for replay determinism and to distinguish stale alternate chains. |

### nFlags retirement

Above activation, the legacy `nFlags` field is **removed** from `CBlockHeader`. The PoS marker moves to `nVersion` bit 30:

```
PoS bit  = (nVersion >> 30) & 0x1
```

This eliminates the ppcoin "do not hash nFlags" serialization quirk in [vericoin/src/serialize.h](../../src/serialize.h) (`SER_POSMARKER`). The marker is now part of the hashed header, closing a long-standing inconsistency.

### Activation

- Below `BinaryChainHeight_*` on each chain: legacy 80-byte header serialization.
- At or above activation: 180-byte extended serialization.
- A node with extended-aware code MUST detect the format by block height and choose the correct serialization path.

### Validation rules

For any block at height `h >= BinaryChainHeight`:

1. The 180-byte serialization must round-trip exactly.
2. `pairedAnchorRef` MUST equal the hash of the latest activated Joint Anchor known to the validating node.
3. `beaconRef` MUST equal the deterministic beacon for `epochIndex` per DACE-2.
4. `rewardAccumulatorRoot` MUST equal the running root from this node's own escrow pool accounting through the previous epoch.
5. `epochIndex == floor((h - BinaryChainHeight) / BEACON_EPOCH_VRC)` for VRC blocks; `epochIndex` is read from the VRM `nTime` projection for VRM blocks (see DACE-2 §4).

Any mismatch invalidates the block.

### Genesis backwards compatibility

Pre-activation blocks remain valid with legacy 80-byte headers. Hash functions on those blocks continue to consume the legacy serialization. No retroactive rehashing.

## Rationale

### Why three fields, not one

Splitting prevents implementations from masking ambiguity inside a Merkle blob. Each field has a single, narrow validation rule. Future protocol versions can add new fields without breaking existing field semantics.

### Why `epochIndex` is explicit

Without it, replay determinism for stale-coupling mode is harder: a node receiving a block out of order needs to know which epoch the block thought it was in without having to query the local chain. Putting it in the header pins it.

### Why retire `nFlags`

Hard-fork activation is the natural moment to clean up the long-standing "not in hash" quirk. The PoS marker now contributes to the header hash, removing one source of cross-implementation hash drift.

## Reference C++ representation

See:
- [vericoin/src/primitives/block.h](../../src/primitives/block.h) — extended header definition
- [vericoin/src/primitives/block.cpp](../../src/primitives/block.cpp) — extended header hash pipeline
- [vericoin/src/dace/header.h](../../src/dace/header.h) — DACE-aware helpers
- [vericoin/src/dace/header.cpp](../../src/dace/header.cpp) — activation-gated serialization

## Test vectors

Test vectors are distributed as JSON in [vericoin/test/dace/header_vectors.json](../../test/dace/header_vectors.json) and exercised by the regtest harness DACE-T01 through DACE-T08.
