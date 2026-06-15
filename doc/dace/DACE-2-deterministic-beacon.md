# DACE-2: Deterministic Height-Based PoW Beacon

Status: Draft
Type: Standards Track (consensus, hard fork)
Created: 2026-05-24

## Abstract

DACE-2 defines how VeriCoin nodes select a Verium proof-of-work block to serve as the entropy and security anchor for a given coupling epoch. Selection is height-based, requires `K_BEACON` confirmations, uses a bounded deterministic fallback ladder on reorgs, and never consumes timestamps as inputs.

## Motivation

Timestamps invite miner discretion and reintroduce beacon-grinding surface. Identity-based selection (e.g., "first miner to publish a coinbase script with prefix X") is gameable. Height-based selection with a depth requirement is replayable from headers alone.

## Specification

### Beacon target height

For coupling epoch `e`:

```
H_e = H_0 + e * DELTA
```

where:
- `H_0` is the VRM height of the first beacon, defined at activation as the smallest VRM height `>= BinaryChainHeight_VRM` divisible by `DELTA`.
- `DELTA` is the VRM block stride from [DACE-0](DACE-0-overview-and-constants.md).

### Candidate selection

The candidate is the first VRM block on the canonical VRM chain at height `>= H_e`:

```
candidate = ChainActive(VRM)[H_e]
```

If `H_e` is not present on the canonical chain (e.g., the chain hasn't reached that height yet), the beacon is **undecided** and no Joint Anchor for epoch `e` may yet be certified.

### Confirmation requirement

`candidate` is eligible to be `beacon_e` only when:

```
ChainActive(VRM).Height() - candidate.nHeight >= K_BEACON
```

Until that depth is reached, the beacon is *observable* but not *certifiable*.

### Fallback ladder

If, before activation in a Joint Anchor, the canonical VRM tip reorgs past `candidate`, selection retries deterministically:

```
for offset in [0, 1, 2, ..., W_FALLBACK]:
    candidate = ChainActive(VRM)[H_e + offset]
    if candidate exists and ChainActive(VRM).Height() - candidate.nHeight >= K_BEACON:
        beacon_e = candidate
        break
if no candidate qualified within W_FALLBACK:
    beacon_e = undecided  # epoch e cannot be anchored yet
```

The ladder is bounded so selection terminates in finite time. If `W_FALLBACK` is exhausted, the epoch's beacon remains undecided and the coupling stalls for that epoch (per [DACE-7](DACE-7-ibd-and-stale-coupling.md) stale-coupling rules).

### Activation lock

Once `beacon_e` is included in a Joint Anchor that becomes **activated** (per [DACE-4](DACE-4-joint-anchors.md)), the beacon is fixed for epoch `e` forever. Further VRM reorgs do not change it. This is the "ratchet" property.

### Epoch index on the VRM side

VRM blocks at height `h_v >= BinaryChainHeight_VRM` carry `epochIndex` projected from VRM nTime:

```
epochIndex_VRM = floor((h_v - BinaryChainHeight_VRM) / DELTA)
```

This lets VRM-side validators verify that the `beaconRef` carried in a VRM block's `pairedAnchorRef` chain corresponds to the right epoch without requiring VRC chain state.

### Depth proof object

When a node relays beacon information to a peer that does not yet have synced VRM headers, it includes a `BeaconDepthProof`:

```
struct BeaconDepthProof {
    uint256 beacon_hash;
    uint32_t beacon_height;
    vector<BlockHeader> succeeding_headers;   // K_BEACON consecutive headers building on beacon
}
```

The receiver validates by checking PoW on each succeeding header and verifying the chain links back to `beacon_hash`.

## Mining-surface impact on VRM

**Zero.** A beacon is a *selection* over normal VRM blocks performed by validators reading headers. VRM miners do not produce any new proof object, signature, or artifact related to beacons. Beacon depth proofs are chains of standard VRM headers anyone with the chain can assemble. `GetWorkHash` (scrypt²) is unchanged.

## Rationale

### Why height, not time

Height is objective; time is locally manipulable. With variable-blocktime VRM, height-based beacons are still deterministic — they just take a varying wall-clock interval.

### Why deep confirmations

`K_BEACON` makes orphaning a beacon prohibitively expensive (it requires reorganizing `K_BEACON` blocks of accumulated work). At balanced parameters (`K_BEACON = 50`), this is ~3–8 hours of VRM PoW.

### Why a bounded fallback ladder

Reorgs at the beacon height before activation can leave the beacon undecided indefinitely if we wait for the *original* height to re-stabilize. The ladder lets selection advance into the next eligible VRM block. The bound `W_FALLBACK` prevents the ladder from drifting arbitrarily far from `H_e` and reintroducing time-based ambiguity.

## Reference implementation

- [vericoin/src/dace/beacon.h](../../src/dace/beacon.h)
- [vericoin/src/dace/beacon.cpp](../../src/dace/beacon.cpp)

## Test vectors

`vericoin/test/dace/beacon_vectors.json` covers:
- Happy path: candidate exists, attains `K_BEACON` confirmations, becomes beacon.
- Reorg before activation: candidate orphaned, fallback ladder selects next.
- Reorg after activation: beacon does not change.
- Tip lags: epoch undecided; no JA certifiable.
- Fallback ladder exhausted: epoch undecided; stale-coupling triggered.
