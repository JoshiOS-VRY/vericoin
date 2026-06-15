# Binary Chain v2 — ASIC/GPU Resistance and Hardware Requirements

> Consensus/design-principle layer. Ties to the existing
> `vericoin/doc/dace/powt-mining-philosophy.md` and `vericoin/doc/dace/cpu-fairness-check.md`.

## 1. Core design principle

> **No DACE, degradation, jackpot, or Binary Address feature may introduce anchor work
> that materially changes the CPU-oriented PoWT mining profile.** Verium contributes
> security through ordinary Verium PoWT mining; VeriCoin contributes security through
> ordinary VeriCoin PoST staking/certification. Anchor participation must be
> commodity-hardware friendly.

Stated as concrete requirements:

- No specialized hardware is required to participate in anchors.
- Anchor participation is possible with normal wallet / full-node software.
- No SNARK / VDF / proof-generation requirement is introduced in v1 (these can favor
  GPUs/ASICs and add cryptographic novelty risk).
- Verium keeps contributing ordinary PoWT work (scrypt², memory-hard, CPU-oriented).
- VeriCoin keeps contributing ordinary PoST staking/certification.
- DACE/degradation/jackpot must not create a new hardware race around anchor formation.

## 2. Why the hybrid does not break Verium's identity

| Activity | Who does it | Hardware |
|----------|-------------|----------|
| Produce VRM beacon | Ordinary Verium miners | Same CPU PoWT mining as today |
| Certify Joint Anchor | VeriCoin bonded ticket operators | Sign with a normal key; no mining |
| Activate anchor | Deterministic, from chain data | None (any full node) |
| Redeem reward claim | Any wallet | None |
| Jackpot eligibility/seed | Indexer/app | None (hashing only, off-chain) |
| Binary Address | Wallet library | None |

The committee certificate is an **ECDSA signature** (Phase 1) by a bonded staker — not a
proof-of-work artifact. Activation is deterministic. The jackpot seed is a single hash of
already-finalized values computed off-chain. None of these create a hardware advantage.

## 3. The header-hashing cost (and why it is acceptable)

DACE's extended header adds ~100 bytes and **one extra SHA256d** per hash attempt, folded
in via `daceMerkleRoot = SHA256d(merkleRoot || pairedAnchorRef || beaconRef ||
rewardAccumulatorRoot || epochIndex)`. Critically, this is **outside** the scrypt² memory-
hard inner loop (`GetWorkHash`). The PoWT identity comes from the memory-hard scrypt² work;
adding a fixed-cost SHA256d in the cold path does not shift the CPU-vs-GPU-vs-ASIC balance.

## 4. The project's own benchmark gate (cited, not a universal proof)

The existing `cpu-fairness-check.md` defines a **Phase 1 gate**: the extended-header hash
must be **≤ 1.05× (5%) slower** than the legacy-header hash across CPU tiers (hobbyist
i5/Ryzen3, prosumer i7/Ryzen7, datacenter Xeon/EPYC), benchmarked via
`DaceHeaderHashLegacy` vs `DaceHeaderHashExtended` in `vericoin/src/bench/dace_header_hash.cpp`.

> The `≤ 1.05×` figure is **the project's own internal benchmark gate**, not a universal
> proof of ASIC/GPU resistance. It demonstrates that the DACE header change does not
> materially alter the mining profile; it does **not** claim Verium is ASIC-proof or
> GPU-proof in any absolute sense.

`powt-mining-philosophy.md` further sets evaluation thresholds (CPU-fairness target ≤2×,
hard ceiling ≤4×; no order-of-magnitude ASIC advantage; large memory-hard/latency-bound
working set; botnet/decentralization analysis) and a **default-to-keep** policy: if a
proposed PoWT change fails evaluation, keep current scrypt².

## 5. Rules for any future anchor-related work

Any proposal that touches the per-hash hot path must:

1. Provide before/after benchmarks on all CPU tiers (≤1.05× gate).
2. Provide GPU-to-CPU and ASIC-to-CPU ratio measurements (no worse than before; target
   ≤2×, ceiling ≤4×).
3. Show the memory profile is unchanged (still latency-bound, large working set).
4. Include a botnet-profitability and miner-decentralization analysis.
5. Justify necessity and provide a reversion plan.
6. Pass the `powt-mining-philosophy.md` conformance checklist.

VDF/VRF gates and private committee self-reveal are **evaluation-only, Phase 4+**, and only
after ≥12 months of mainnet data and a full BIP cycle — never mandatory in v1.

## 6. Summary

Binary Chain v2 adds coupling, degradation, rewards, jackpot, and a dual-chain address
**without** adding any specialized-hardware requirement to anchor formation. The only
per-hash cost is a fixed SHA256d in the cold path, gated by the project's existing ≤1.05×
benchmark. Commodity hardware remains sufficient for full participation.
