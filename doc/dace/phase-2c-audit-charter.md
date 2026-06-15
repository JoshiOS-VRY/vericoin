# Phase 2c — Audit and Bug Bounty Charter

Status: Draft
Phase: 2c (external audit + time-boxed bug bounty)
Duration: 6–8 weeks
Prerequisite: Phase 2b clean run for 90 days

## Objective

Subject the DACE codebase, specifications, and testnet behavior to external review and a public bug bounty before any mainnet activation height is committed.

## Audit scope

External reviewers receive:

1. **Specifications**: DACE-0 through DACE-7 + the design guide + this charter.
2. **Code**: the `vericoin/src/dace/` module, modifications to `pos.cpp` for kernel coupling, `primitives/block.{h,cpp}` for the extended header, `chainparams.cpp` for activation gating, and `rpc/dace.cpp` for the RPC surface.
3. **Tests**: `vericoin/test/functional/dace_*.py` plus `vericoin/src/bench/dace_header_hash.cpp`.
4. **Testnet data**: 90 days of `binarychain_metrics` snapshots, the running decision log from Phase 2a/2b, and all closed Critical / High bug reports.
5. **Threat model**: Section 13.1 of the design guide.

Reviewers are explicitly tasked with evaluating:

| Area | Specific questions |
|------|---------------------|
| Header serialization | Are there any encoding ambiguities? Round-trip determinism. Backwards-compat with pre-fork blocks. |
| Hash pipeline | Does the daceMerkleRoot substitution preserve PoW semantics? Any preimage / collision concerns? |
| Beacon selection | Is the fallback ladder genuinely deterministic? Race conditions with paired-chain reorgs? |
| Sortition | Without-replacement bias? Tiebreaker correctness? Splitting-attack resistance. |
| Joint Anchor lifecycle | Activation race conditions? Could two anchors activate simultaneously? Slashing evidence completeness. |
| Reward accumulator | Merkle root reconstruction equivalence to ComputeMerkleRoot. Leaf ordering determinism. |
| Claim redemption | Replay protection. Expiry. Escrow drain attacks. |
| Kernel coupling | Stake-grinding analysis with the new modMix. Does any existing PoST attack still apply? |
| Stale-coupling | Can a node be tricked into entering or leaving stale mode? Are there liveness implications? |
| Recovery quorum | Bribery analysis. Bonded-stake threshold correctness. |
| Activation | Coordinated dual-chain hard fork choreography. What happens if one chain hits activation height significantly before the other? |
| Identity invariants | Are any of the Section 2.4 invariants accidentally violated by the implementation? |
| CPU fairness | Is the Phase 1 fairness check valid? Are there hardware classes that benefit disproportionately from the daceMerkleRoot extension? |

## Audit deliverable

- Per-issue severity (Critical / High / Medium / Low / Informational) with reproduction steps.
- A high-level architecture review with up to 5 design improvement recommendations.
- A clean-pass attestation: "We reviewed the above scope and identified no Critical or High issues that would prevent mainnet activation."

## Bug bounty

- Time-boxed to 4 weeks running in parallel with the external audit's final 4 weeks.
- Scope: same as the audit scope plus the desktop wallet's DACE pages.
- Reward bands: Critical $40k–$100k, High $10k–$40k, Medium $2k–$10k, Low $500–$2k. (Adjust to project budget.)
- Public scope, public submissions tracker, private disclosure process for Critical findings.

## Gating criteria for Phase 3a (mainnet soft activation)

To proceed to mainnet:

1. External audit returns a clean-pass attestation, OR all returned Critical/High issues are fixed and re-reviewed.
2. Bug bounty window closes with zero unresolved Critical findings.
3. Fix-rollback: every Critical/High fix has its own test added to the regtest suite.
4. Re-running Phase 2b adversarial drills on the post-audit code base passes cleanly.

If any criterion fails, return to Phase 2b until the gates are met.

## Post-audit publication

After Phase 3 mainnet activation succeeds (Phase 3 §"Decision log"), publish:
- The audit firm's report (with consent).
- The list of bug bounty findings with severity, fix commit, and bounty paid.
- A retrospective document.

This becomes part of the project's public security record.
