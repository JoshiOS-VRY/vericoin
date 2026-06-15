# Binary Chain v3 (DACE) Specifications

**DACE** = Dual-Anchor Coupled Epochs.

This directory contains the consensus and protocol specifications for Binary Chain v3, the dual-chain coupling between VeriCoin (VRC, PoST) and Verium (VRM, PoWT).

## Core safety rule

> Foreign-chain information may be observed and certified in the current epoch but may only affect local block validity after delayed activation.

## Specifications

| ID | Title | Status |
|----|-------|--------|
| [DACE-0](DACE-0-overview-and-constants.md) | Overview and Frozen Constants | Draft |
| [DACE-1](DACE-1-extended-block-header.md) | Extended Block Header | Draft |
| [DACE-2](DACE-2-deterministic-beacon.md) | Deterministic Height-Based PoW Beacon | Draft |
| [DACE-3](DACE-3-bonded-ticket-committee.md) | Bonded Ticket Registry and Committee Sortition | Draft |
| [DACE-4](DACE-4-joint-anchors.md) | Joint Anchors and Three-Phase Lifecycle | Draft |
| [DACE-5](DACE-5-delayed-reward-accumulators.md) | Delayed Cross-Chain Reward Accumulators | Draft |
| [DACE-6](DACE-6-p2p-additions.md) | P2P Protocol Additions (xheaders / ja / jasig / claim) | Draft |
| [DACE-7](DACE-7-ibd-and-stale-coupling.md) | Initial Block Download and Stale-Coupling | Draft |

## Governance

- [PoWT Mining Philosophy Governance](powt-mining-philosophy.md) — Section 18 of the design guide, published as standalone governance.

## Phase runbooks

- [Phase 2a: Shadow Mode](phase-2a-shadow-mode-runbook.md)
- [Phase 2b: Testnet Finality](phase-2b-testnet-finality-runbook.md)
- [Phase 2c: Audit and Bug Bounty Charter](phase-2c-audit-charter.md)
- [Phase 3a: Mainnet Soft Activation](phase-3a-soft-activation-runbook.md)
- [Phase 3b: Mainnet Hard Activation](phase-3b-hard-activation-runbook.md)
- [Phase 4: Repo Unification](phase-4-repo-unification.md)
- [Phase 4: VRF/VDF Hardening Evaluation](phase-4-hardening-vrf-vdf.md)

## Reading order

1. **DACE-0** to understand constants and the overall architecture.
2. **DACE-1, DACE-2, DACE-3, DACE-4, DACE-5** for the consensus rules in dependency order.
3. **DACE-6, DACE-7** for networking and IBD.
4. Governance + phase runbooks for deployment context.
