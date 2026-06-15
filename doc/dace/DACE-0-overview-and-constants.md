# DACE-0: Overview and Frozen Constants

Status: Draft
Type: Standards Track / Informational
Created: 2026-05-24

## Abstract

This document specifies the high-level architecture of Binary Chain v3 (DACE — Dual-Anchor Coupled Epochs) and freezes the consensus constants that govern the protocol. All other DACE specifications reference the constants defined here.

## Motivation

VeriCoin (VRC) is a PoST payments chain. Verium (VRM) is a PoWT reserve chain. The Binary Chain concept is to couple them at the consensus layer so that each chain contributes security to the other without either becoming a sidecar of the other. DACE achieves this through a single safety rule:

> Foreign-chain information may be observed and certified in the current epoch but may only affect local block validity after delayed activation.

## Architecture summary

DACE introduces four consensus objects:

1. **Deep PoW Beacon** (`B_e`) — a VRM block selected by height that has attained `K_BEACON` confirmations.
2. **Bonded Ticket Registry** — fixed-size locked stake units on VRC sampled without replacement.
3. **Joint Anchor** (`JA_e`) — a record signed by the VRC ticket committee, referencing a VRM beacon. Three-phase lifecycle: observed → certified → activated.
4. **Reward Accumulators** — local escrow pools on each chain whose Merkle roots are anchored by JAs and redeemed by pull-based claims.

## Frozen consensus constants

Constants are profile-selectable for testnet but **frozen** for mainnet. The mainnet profile defaults to **Balanced**.

### Epoch and beacon

| Constant | Conservative | Balanced (mainnet default) | Aggressive | Notes |
|----------|-------------:|---------------------------:|-----------:|-------|
| `DELTA` | 18 VRM blocks | 12 VRM blocks | 8 VRM blocks | Beacon stride (~hour at variable VRM blocktime) |
| `K_BEACON` | 100 | 50 | 25 | Confirmations before a candidate becomes a beacon |
| `W_FALLBACK` | 6 | 6 | 4 | Bounded fallback ladder window |
| `BEACON_EPOCH_VRC` | 60 VRC blocks | 60 VRC blocks | 60 VRC blocks | Coupling epoch length (always ~60 minutes on VRC) |

### Ticket registry

| Constant | Conservative | Balanced (mainnet default) | Aggressive | Notes |
|----------|-------------:|---------------------------:|-----------:|-------|
| `STAKE_UNIT_VRC` | 1000 VRC | 1000 VRC | 500 VRC | Fixed-size locked stake per ticket |
| `L` (lockup) | 8 epochs | 6 epochs | 4 epochs | ~8h, ~6h, ~4h |
| `UNBOND_DELAY` | 2 epochs | 2 epochs | 1 epoch | Withdraw becomes spendable after this |

### Committee

| Constant | Conservative | Balanced (mainnet default) | Aggressive | Notes |
|----------|-------------:|---------------------------:|-----------:|-------|
| `M` (committee size) | 256 | 128 | 64 | Tickets selected per epoch |
| `q` (quorum) | 2/3 | 2/3 | 2/3 | Signature threshold |
| `RECOVERY_THRESHOLD` | 0.80 | 0.80 | 0.75 | Bonded supermajority for recovery anchor |

### Finality and recovery

| Constant | Conservative | Balanced (mainnet default) | Aggressive | Notes |
|----------|-------------:|---------------------------:|-----------:|-------|
| `S_GRACE` | 4 epochs | 3 epochs | 2 epochs | Stale-coupling grace before warning |
| `S_MAX` | 32 epochs | 16 epochs | 8 epochs | Maximum stall before recovery anchor allowed |
| `FINALITY_WINDOW` | 6h | 6h | 3h | Signature assembly window per anchor |

### Economic divert bands

| Constant | Value | Notes |
|----------|------:|-------|
| `σ` (VRM diverts to VRC pool, fraction of base subsidy) | 4% | Frozen for mainnet |
| `φ` (VRC diverts to VRM pool, fraction of block fees) | 10% | Frozen for mainnet |

Divert bands are intentionally small and asymmetric to reflect VRM as a subsidy-rich PoW chain and VRC as a fee-bearing payments chain. Reward bands may only be changed by a hard fork; no live oracle ever enters consensus.

### Activation

| Constant | Value | Notes |
|----------|------:|-------|
| `BinaryChainHeight_VRM` | TBD at Phase 0 close | Frozen 120+ days before activation |
| `BinaryChainHeight_VRC` | TBD at Phase 0 close | Frozen 120+ days before activation, coordinated with VRM target |
| `BC_PROTOCOL_VERSION` | `90100` | Minimum peer protocol version above activation |
| `DACE_BOOTSTRAP_VRM_HEADER` | TBD at activation | Hash of VRM block at `BinaryChainHeight_VRM - 1` |
| `DACE_BOOTSTRAP_VRC_HEADER` | TBD at activation | Hash of VRC block at `BinaryChainHeight_VRC - 1` |
| `DACE_BOOTSTRAP_JA_GENESIS` | TBD at activation | Hash of initial JA_0 (including initial committee root and beacon) |

## Header hash function

- VRC continues to use `scryptHash` (per [vericoin/src/primitives/block.cpp](../../src/primitives/block.cpp) `GetWorkHash`).
- VRM continues to use `scryptHash` (scrypt² with N = 1024², ~128 MB per thread).
- **DACE does not change either hash function.** The extended header adds approximately 100 bytes of input data inside the hashed region. This is negligible relative to scrypt²'s memory-hard work.

See [powt-mining-philosophy.md](powt-mining-philosophy.md) for the governance rule that locks this in.

## Identity invariants

Per Section 2.4 of the design guide, DACE preserves these invariants:

- VeriCoin remains the fast PoST payments/staking chain.
- Verium remains the PoWT reserve/commodity/mining chain with CPU-fair mining.
- Neither chain is a sidecar.
- Joint Anchors require **both** halves (VRC committee signature **and** buried VRM PoW beacon).
- Verium security contribution is objective accumulated work, never identity counts.
- VeriCoin security contribution is bonded stake, never live foreign-chain validation.
- Cross-chain economic coupling is mandatory and delayed-pull-based.

## References

- Design guide: `binary_chain_spec_84a31810.plan.md`
- Deep research critique: `deep-research-report.md`
- VeriCoin PoST whitepaper: [VeriCoinPoSTWhitePaper10May2015.pdf](../../VeriCoinPoSTWhitePaper10May2015.pdf)
