# DACE-5: Delayed Cross-Chain Reward Accumulators

Status: Draft
Type: Standards Track (consensus, hard fork)
Created: 2026-05-24

## Abstract

DACE-5 specifies how each chain pays out cross-chain rewards in its **native currency** without ever depending on live foreign-chain payee data inside a current block. Each chain accumulates claim deltas into a local Merkle root, anchors that root via a Joint Anchor, and lets recipients on the other chain redeem via pull-based Merkle proof.

## Motivation

v2 required block validity to depend on live foreign payee data ("the next reward output must pay this specific live-tip payee from the other chain"). That is the textbook circular-validation trap: honest nodes with different paired-chain views could disagree about validity. DACE removes this entirely.

## Specification

### Local escrow pools

Each chain maintains a designated escrow output type. Each block above activation diverts:

- **VRM** → VRC pool: `σ * base_subsidy` of each VRM block's subsidy.
- **VRC** → VRM pool: `φ * block_fees` of each VRC block's fees.

Divert bands `σ` and `φ` are frozen in [DACE-0](DACE-0-overview-and-constants.md).

The diverted amount is paid in the local chain's currency into an escrow scriptPubKey. The escrow scriptPubKey is consensus-defined and unspendable except via valid claim redemption transactions (see below).

### Claim leaves

Each block's coinbase (VRM) or coinstake (VRC) contains a **claim delta** entry: an ordered list of leaves describing who is owed how much, drawn from the activated JA's view of the paired chain.

For a VRM block (paying VRC committee operators):

```
leaf = {
    source_chain:   VRM,
    source_block:   this_block.hash,
    epoch:          current_epoch,
    recipient_kind: VRC_TICKET_OPERATOR,
    recipient_pkh:  H(operator_pubkey),    // from selected ticket of the activated JA
    amount:         per_ticket_share,
}
```

For a VRC block (paying VRM beacon miners):

```
leaf = {
    source_chain:   VRC,
    source_block:   this_block.hash,
    epoch:          current_epoch,
    recipient_kind: VRM_BEACON_MINER,
    recipient_pkh:  H(beacon_coinbase_script), // from the activated JA's beacon
    amount:         beacon_share,
}
```

### Per-epoch accumulator root

Each chain runs a running Merkle accumulator of leaves committed during the epoch. At epoch close, the running root becomes the epoch's `rewardAccumulatorRoot_e`.

That root is committed in two places:
1. The `rewardAccumulatorRoot` field of every block in epoch `e+1` (immutable replay reference for IBD).
2. The `reward_root_*_prev` fields of `JA_{e+1}` (anchored for cross-chain redemption).

### Pull-based redemption

A claim is redeemed by a transaction on the **destination** chain:

```
claim_tx {
    inputs:      [the destination chain's escrow output]
    outputs:     [pay to recipient_pkh, amount]
    witness:     {
        epoch:       u32,
        ja_hash:     uint256,    // activated JA that anchored the reward_root
        merkle_path: vector<uint256>,
        leaf:        ClaimLeaf,
    }
}
```

Validation:
1. `ja_hash` MUST be the hash of an activated Joint Anchor known to this chain.
2. The corresponding `reward_root_*_prev` from that JA MUST be reconstructable from `leaf` + `merkle_path`.
3. The leaf MUST have `recipient_pkh == output[0].scriptPubKey.pkh`.
4. The leaf's epoch MUST not already have been redeemed by this claimant (single-use, tracked in a UTXO-like claim-spent set).
5. The escrow MUST hold sufficient balance.

No block-validity rule ever depends on live foreign payee data. The block creating the reward never needed to know the future claimant.

### Caps and expiry

- `CLAIM_EXPIRY = 1024 epochs`. Unredeemed claims after this window are forfeited; escrow balance reverts to the chain treasury (a deterministic burn or a consensus-defined fund — frozen at Phase 0).
- No per-block claim count cap is needed initially; the leaves are small (~80 bytes each) and bounded by committee size and beacon cadence.

### Asymmetry analysis

Per [DACE-0](DACE-0-overview-and-constants.md) economic model, asymmetric value transfer is acknowledged and not corrected for via consensus. Let `ρ = P_VRM / P_VRC`:

| ρ | VRC committee APR boost | VRM miner revenue boost |
|---:|---:|---:|
| 0.25 | 0.20% | 10.00% |
| 1.00 | 0.80% | 2.50% |
| 4.00 | 3.20% | 0.63% |

Adjust bands by governance hard fork if asymmetry becomes politically unacceptable. Never import a price oracle into consensus.

## Rationale

### Why pull-based, not push-based

Push-based rewards require the paying block to know the recipient. The recipient depends on live foreign-chain state. That's the circular-validation trap. Pull-based redemption decouples: the paying block commits to an opaque accumulator root; the recipient later proves inclusion at their leisure.

### Why local currency, not wrapped

A bridge / wrapping mechanism creates honeypots and custody surface. Native-currency reward keeps the design free of bridges.

### Why per-epoch root, not per-block

Per-block root would require constant Merkle updates and constant JA churn. Per-epoch root batches claims efficiently with one JA anchoring per epoch.

## Reference implementation

- [vericoin/src/dace/reward_accumulator.h](../../src/dace/reward_accumulator.h)
- [vericoin/src/dace/reward_accumulator.cpp](../../src/dace/reward_accumulator.cpp)
- [vericoin/src/dace/claim.h](../../src/dace/claim.h)
- [vericoin/src/dace/claim.cpp](../../src/dace/claim.cpp)

## Test vectors

`vericoin/test/dace/claims_vectors.json` covers:
- Single-leaf accumulator: root reconstruction.
- Multi-leaf accumulator: out-of-order leaves sorted deterministically.
- Claim redemption: valid Merkle path produces accepted tx; invalid path rejected.
- Replay attack: a redeemed claim cannot be redeemed again.
- Expiry: a claim past `CLAIM_EXPIRY` is rejected; escrow balance forfeited correctly.
