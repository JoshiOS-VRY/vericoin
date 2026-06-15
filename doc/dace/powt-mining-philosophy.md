# Verium PoWT Mining Philosophy — Governance Policy

Status: Active governance policy
Last updated: 2026-05-24

Verium's CPU-fair mining model is part of the product, not an implementation detail. This policy lays out the criteria any proposed change to Verium's proof-of-work must satisfy before it can be considered for inclusion in a future Binary Chain version. It corresponds to Section 18 of the design guide.

## Identity invariants (reference)

From [DACE-0 §"Identity invariants"](DACE-0-overview-and-constants.md):

- VeriCoin remains the fast PoST payments/staking chain.
- Verium remains the PoWT reserve/commodity/mining chain.
- Neither chain is a sidecar.
- Joint Anchors require both halves (VRC bonded-ticket committee signature AND buried VRM PoW beacon).
- Verium's security contribution is objective accumulated work, not gameable identity counts.
- VeriCoin's security contribution is bonded stake, not live foreign-chain validation.
- Cross-chain coupling is mandatory and delayed-pull-based.

## 1. Evaluation criteria

| Criterion | What to measure | Threshold |
|-----------|-----------------|-----------|
| **CPU fairness** | Throughput per dollar across at least three reference CPU tiers vs equivalent GPU and ASIC investment | No hardware class may exceed CPU economics by more than a small factor (target ≤ 2×, hard ceiling ≤ 4×) |
| **GPU resistance** | GPU-to-CPU speedup ratio under realistic memory subsystem assumptions | ≤ 2× target; ≤ 4× hard ceiling |
| **ASIC resistance** | Plausible silicon area and power advantage for an FPGA or ASIC implementation, including amortized NRE | No order-of-magnitude advantage; explicit memory-hard or memory-latency-bound design |
| **Memory profile** | Working set per mining thread; bandwidth and latency sensitivity | Working set large enough to defeat trivial parallelization; latency-bound to preserve the memory-hard property |
| **Botnet risk** | How attractive is opportunistic compromise of consumer or cloud machines vs honest mining | Reward must not make stolen-cycle mining materially more profitable than honest mining; consider memory and power constraints typical of compromised hosts |
| **Miner decentralization** | Plausible distribution of hashpower across hobbyist, prosumer, datacenter | Must remain feasible for a hobbyist with consumer hardware to mine at break-even at network difficulty |
| **Pool centralization risk** | Whether the design encourages or discourages large pools | Variance and reward smoothness must not require pooling for hobbyists |
| **Hot-path additions** | Any new work added to the per-hash inner loop (signing, beacon validation, VDF, SNARK, aggregation, foreign-state reads) | **None permitted in the hot path.** Cold-path work outside `GetWorkHash` may be acceptable. |

## 2. Required process

Any PoWT change proposal must include:

1. A reference implementation of the new hash function with reproducible benchmarks across at least one CPU, one GPU, one FPGA estimate, and one ASIC estimate.
2. A memory-profile measurement (working set, bandwidth, latency sensitivity).
3. A botnet-risk analysis using realistic stolen-resource economics.
4. A miner-decentralization simulation under current and projected network difficulty.
5. A justification that the change is necessary, not merely modernizing.
6. A reversion plan if mainnet data contradicts pre-launch evaluation.
7. A re-affirmation of the Section 2.4.3 conformance checklist:

   1. Does it preserve VeriCoin's role as the fast PoST payments chain?
   2. Does it preserve Verium's role as the PoWT reserve/commodity chain?
   3. Does it preserve independent operation of both chains under stale-coupling?
   4. Do Joint Anchors still require both halves (stake certificate + buried PoW beacon)?
   5. Is Verium's security contribution still based on objective accumulated work, not identity counts?
   6. Is VeriCoin's security contribution still based on bonded stake, not live foreign reads?
   7. Does cross-chain coupling remain mandatory and delayed-pull-based?
   8. Does Verium PoWT remain CPU-fair per Section 18 (this document)?

## 3. Default position

If a proposed PoWT change does not pass all of Section 1 with documented evidence, **the default is to keep current scrypt²**.

The DACE design assumes this default. It does not require any change to `GetWorkHash` in [vericoin/src/primitives/block.cpp](../../src/primitives/block.cpp) or the standalone [verium/src/primitives/block.cpp](../../../verium/src/primitives/block.cpp). The DACE-1 extended header is committed to via the daceMerkleRoot substitution in the existing 80-byte scrypt² input — no scrypt² SIMD-layer changes are needed.

## 4. What DACE itself is exempt from

DACE adds approximately 100 bytes of input to one extra SHA256d per hash attempt (the daceMerkleRoot computation). Per Section 1 of this policy:

- This is a cold-path-equivalent addition (an extra SHA256d at the start of the hash, not inside the scrypt² memory-hard inner loop).
- The threshold "no hot-path additions" is met: scrypt²'s inner loop is unchanged.
- The threshold "CPU fairness ≤ 1.05× slowdown" is the Phase 1 gate — see [cpu-fairness-check.md](cpu-fairness-check.md).

Implementations must run [cpu-fairness-check.md](cpu-fairness-check.md) before testnet activation.

## 5. Amendment process

This policy is amended only by a governance proposal that itself satisfies the conformance checklist in Section 2.7. Amendments require:

- A 30-day public comment period.
- Approval from the DACE governance signatories (defined in the project's governance document).
- A reference to which paragraphs change and why.

Trivial editorial corrections (typo fixes, link updates) do not require the above process.
