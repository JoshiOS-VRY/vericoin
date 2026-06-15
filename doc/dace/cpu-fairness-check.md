# CPU-Fairness Check Procedure

This document defines the **required Phase 1 gate** before DACE testnet activation.

## Goal

Confirm that the DACE-1 extended header does not measurably reduce scrypt² mining throughput on Verium. Per Section 2.4.2 of the design guide:

- VRM's `GetWorkHash` continues to consume an 80-byte scrypt² input.
- DACE-1 substitutes the merkle root slot with `daceMerkleRoot = SHA256d(merkleRoot || pairedAnchorRef || beaconRef || rewardAccumulatorRoot || epochIndex)`.
- Added work per hash: one extra SHA256d over ~132 bytes.

## Threshold

**Maximum allowed slowdown vs legacy: 1.05× (5%).**

If exceeded, the implementation has regressed and must be investigated before testnet activation.

## Reference benchmark

The bench binary contains two cases:

- `DaceHeaderHashLegacy` — legacy 80-byte scrypt² hash with extended fields zeroed.
- `DaceHeaderHashExtended` — DACE-1 extended layout with non-zero extended fields, exercising the daceMerkleRoot substitution path.

Source: [vericoin/src/bench/dace_header_hash.cpp](../../src/bench/dace_header_hash.cpp).

## Running the check

After a clean build with `--enable-bench`:

```bash
# VRM build
$ ./bench/bench_verium --filter='DaceHeaderHash.*' --evals=10 --output_csv=cpu_fairness_vrm.csv

# VRC build
$ ./bench/bench_vericoin --filter='DaceHeaderHash.*' --evals=10 --output_csv=cpu_fairness_vrc.csv
```

Compute the ratio:

```
slowdown = mean(DaceHeaderHashExtended) / mean(DaceHeaderHashLegacy)
```

Required: `slowdown <= 1.05` on **all** of the following reference tiers:

| Tier | Reference CPU |
|------|---------------|
| Hobbyist | Intel i5 mobile / AMD Ryzen 3 mobile, no AVX2 |
| Prosumer | Intel i7 desktop / AMD Ryzen 7, with AVX2 |
| Datacenter | Intel Xeon Gold / AMD EPYC, with AVX512 |

Run the bench on each tier in turn and record the slowdown.

## Cross-hardware comparison

Per Section 18.1 PoWT Mining Philosophy Governance, also benchmark GPU and FPGA estimates:

| Class | Reference | Method |
|-------|-----------|--------|
| GPU | NVIDIA RTX 4090 | Run reference scrypt² on a memory-coalesced GPU implementation if one exists. |
| FPGA | Xilinx Versal AI Core | Estimate from datasheet memory bandwidth. ASIC would be similar. |

DACE preserves CPU-fairness iff the GPU-to-CPU and ASIC-to-CPU ratios after extended-header changes are **no worse** than before. Run the bench in both modes on each platform and confirm.

## Pass criteria

- All CPU tiers: extended/legacy slowdown <= 1.05×.
- GPU-to-CPU ratio: no degradation vs legacy mode (target ≤ 2× speedup, hard ceiling ≤ 4×).
- ASIC-to-CPU ratio: same.
- Memory profile: unchanged (DACE adds only SHA256d, not extra memory pressure).

## Reporting

Submit results to the Phase 1 governance review with:
- The CSV outputs.
- A short writeup documenting CPU/GPU/FPGA hardware versions used.
- Pass/fail conclusion against the thresholds above.

This is the same evidence package described in Section 18.2 of the design guide.
