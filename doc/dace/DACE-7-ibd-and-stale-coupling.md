# DACE-7: Initial Block Download and Stale-Coupling

Status: Draft
Type: Standards Track
Created: 2026-05-24

## Abstract

DACE-7 specifies how nodes synchronize from genesis through and past the activation height while validating cross-chain coupling, and how they behave when paired-chain data is temporarily unavailable.

## Specification

### Two-phase IBD

**Phase A (heights below activation):** legacy validation only. Pre-fork blocks are accepted with the legacy 80-byte header serialization and legacy hash pipeline. No cross-chain checks.

**Phase B (heights at or above activation):** extended header serialization (per [DACE-1](DACE-1-extended-block-header.md)) and full DACE validation:
- `pairedAnchorRef` must equal the hash of the latest activated JA known to this node.
- `beaconRef` must equal the deterministic beacon for `epochIndex` per [DACE-2](DACE-2-deterministic-beacon.md).
- `rewardAccumulatorRoot` must equal the locally-computed running root from prior epoch.

### Bootstrap anchors

Three consensus constants are shipped in `chainparams` at activation:

```
DACE_BOOTSTRAP_VRM_HEADER  = <hash of VRM block at BinaryChainHeight_VRM - 1>
DACE_BOOTSTRAP_VRC_HEADER  = <hash of VRC block at BinaryChainHeight_VRC - 1>
DACE_BOOTSTRAP_JA_GENESIS  = <hash of JA_0>
```

A fresh syncing node uses these to validate the start of Phase B without prior knowledge of the paired chain. After bootstrap, paired-chain header sync proceeds via `getxheaders`/`xheaders`.

### Stale-coupling mode

If a node is missing paired-chain data needed to validate the current local block (paired headers, JA, or beacon proof), it enters **stale-coupling mode**:

```
Local processing:        continues normally (PoW/PoST validation, mempool, etc.)
Finality upgrades:        paused (no new JA may activate locally)
New cross-chain validation: deferred (current block accepted under prior JA_active)
Warning surface:          binarychain_status RPC returns stale = true
```

Stale-coupling is bounded: if it persists for `S_MAX` epochs, a recovery anchor may be assembled per [DACE-4](DACE-4-joint-anchors.md). Until then, no consensus halt.

### Paired-header storage

Each daemon maintains a LevelDB subdb for the paired chain's headers:

- `xheaders/` keyed by `chain_id || height || hash`
- Stores standard `CBlockHeader` (legacy 80-byte for pre-activation pre-fork headers, 180-byte for post-activation headers)
- Total size: ~52 MB/year combined at current chain rates; negligible relative to block bodies

### IBD pacing

Block processing above activation requires paired headers covering up to `tipHeight + COUPLE_LOOKAHEAD = 5` epochs. If paired-header sync lags this window, block processing pauses (not halts) until the gap closes.

### Replay determinism

A node syncing from genesis must produce bit-identical block hashes across implementations. The order of operations is:

1. Sync VRC and VRM headers in parallel (paired-header set permitting).
2. Below activation: validate each chain independently.
3. At activation height: load `DACE_BOOTSTRAP_*` constants from chainparams; validate `JA_0` matches `DACE_BOOTSTRAP_JA_GENESIS`.
4. Above activation: for each new block, run extended-header validation as specified in DACE-1.
5. Joint Anchor activation events are processed deterministically in epoch order; no node-local timing dependency.

## Reference implementation

- [vericoin/src/dace/header_sync.h](../../src/dace/header_sync.h)
- [vericoin/src/dace/header_sync.cpp](../../src/dace/header_sync.cpp)
- [vericoin/src/dace/stale_coupling.h](../../src/dace/stale_coupling.h)
- [vericoin/src/dace/stale_coupling.cpp](../../src/dace/stale_coupling.cpp)
- Hooks into [vericoin/src/validation.cpp](../../src/validation.cpp) and [vericoin/src/net_processing.cpp](../../src/net_processing.cpp)

## Test vectors

`vericoin/test/dace/ibd_vectors.json` covers:
- Cold-sync from genesis across the activation boundary with paired headers in parallel.
- Cold-sync from genesis with paired headers temporarily missing (enters and recovers from stale-coupling).
- Replay from genesis: bit-identical hashes across implementations.
- Bootstrap anchor mismatch: reject IBD and halt with clear error.
