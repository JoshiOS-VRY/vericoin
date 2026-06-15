# Binary Chain v2 — Spec Set

Binary Chain v2 is the current release/spec package for the **Binary Chain** ecosystem:
two sovereign chains — **Verium (VRM, PoWT)** and **VeriCoin (VRC, PoST)** — coupled at the
epoch level by the **DACE** engine, with a **BiCED degradation** overlay, an app-layer
**Binary Jackpot Rewards** adoption layer, and a wallet-layer **Binary Address** descriptor.

This set **extends** the existing canonical DACE specs in [`../dace/`](../dace) (DACE-0..7 +
phase runbooks). Where byte-level layout differs, `../dace/` is the lower-level source of
truth; these docs are the source of truth for the v2 architecture, layering, and adoption
features.

## Read in this order

| # | Doc | Layer | What it covers |
|---|-----|-------|----------------|
| 12 | [12-consensus-boundary.md](12-consensus-boundary.md) | gatekeeper | **Read first.** Classifies every feature into a layer; keeps jackpot/promotion out of consensus |
| 00 | [00-overview-and-recommendation.md](00-overview-and-recommendation.md) | docs | Executive recommendation, naming hierarchy, dev/investor/user framings |
| 01 | [01-dace-core.md](01-dace-core.md) | consensus | DACE engine, constants, objects, full anchor state machine |
| 02 | [02-coupled-degradation-biced.md](02-coupled-degradation-biced.md) | mixed | Degradation ladder + recovery (BiCED integration) |
| 03 | [03-other-chain-header-model.md](03-other-chain-header-model.md) | consensus/node | Paired-header DB, SPV rules, PoST trust caveat, eclipse resistance |
| 04 | [04-reward-architecture.md](04-reward-architecture.md) | consensus | Base-vs-bonus, accumulators, pull claims, expiry — not a bridge |
| 05 | [05-binary-jackpot-rewards.md](05-binary-jackpot-rewards.md) | app/indexer | Jackpot design, pool growth, winner selection, economics, compliance |
| 06 | [06-binary-address.md](06-binary-address.md) | wallet | `vbc1` dual-chain descriptor |
| 07 | [07-asic-gpu-and-hardware.md](07-asic-gpu-and-hardware.md) | design principle | Commodity-hardware / PoWT identity |
| 08 | [08-threat-model.md](08-threat-model.md) | all | Adversarial analysis |
| 09 | [09-implementation-roadmap.md](09-implementation-roadmap.md) | all | Phased rollout aligned to existing runbooks |
| 10 | [10-codebase-mapping-and-rpcs.md](10-codebase-mapping-and-rpcs.md) | all | File touchpoints + RPCs + consensus/app split |
| 11 | [11-open-questions-and-non-goals.md](11-open-questions-and-non-goals.md) | all | Open questions + explicit do-not-do list |

## The five-second model

- **Binary Chain** = public product.
- **DACE** = the epoch-coupling engine (core, already mostly implemented).
- **BiCED** = degradation/recovery overlay (not default per-block coupling).
- **Binary Jackpot Rewards** = adoption engine, **app-layer first**.
- **Binary Address** = wallet UX, **wallet-layer only**.

## Non-negotiable biases

Base rewards are sacred · bonus rewards are programmable · jackpot is app-layer first ·
no USD oracle in consensus · no specialized anchor hardware · no live foreign payee ·
no "lottery" language · no pool/exchange/custodial eligibility for the individual reward ·
no overclaiming ASIC-proofness, legal safety, or attack impossibility.

## Status

These are **specification documents** (Phase 0). The DACE engine is partially implemented in
[`../../src/dace/`](../../src/dace); the largest implementation gap is that
**`verium/src/dace/` does not yet exist** (DACE is VeriCoin-only today). See docs 09 and 10
for the wiring/cleanup prerequisites before testnet.
