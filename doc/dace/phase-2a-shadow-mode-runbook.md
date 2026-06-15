# Phase 2a — Shadow Mode Runbook

Status: Draft
Phase: 2a (testnet shadow mode)
Duration: ~45 days
Prerequisite: Phase 1 regtest sign-off ([shadow-regtest](../../test/functional/) plus [CPU fairness check](cpu-fairness-check.md))

## Objective

Run DACE consensus rules in **shadow mode** on testnet:

- Beacons are selected and observed.
- Reward accumulators are computed.
- Joint Anchor candidates are constructed and gossiped.
- **No consensus effect**: shadow values do not affect block validity. The chain operates under pre-DACE rules.

Goal: prove that beacon selection and accumulator computation produce **bit-identical results** across all nodes over 100,000+ simulated epochs and across live testnet nodes.

## Configuration

Run `vericoind` and `veriumd` testnet builds with:

```
-binarychainheight=<testnet activation height>
-dace-shadow-mode=1
```

`-dace-shadow-mode=1` causes:
- DACE validation to log observations but not reject blocks.
- Reward accumulator roots to be computed but `rewardAccumulatorRoot` is not enforced.
- Beacons selected and surfaced via `binarychain_status` RPC but not consumed by the PoST kernel.

## Required observations

Run for 45 calendar days minimum across a network of:
- ≥ 3 mining nodes on VRM testnet (geographically distributed).
- ≥ 3 staking nodes on VRC testnet.
- ≥ 1 RPC-only observer node on each chain.

Collect from `binarychain_metrics` RPC daily:

| Metric | Pass criterion |
|--------|----------------|
| `divergent_beacon_selections` | **must be 0** across all nodes |
| `reorgs_at_beacon` | Track count, record cause |
| `paired_header_lag_p95_sec` | < `2 * BeaconEpochVRC * 60` seconds |
| `paired_header_lag_p99_sec` | < `4 * BeaconEpochVRC * 60` seconds |
| `ibd_replay_consensus_diffs` | **must be 0** during nightly replay-from-genesis |

## Gating criteria for Phase 2b

To proceed to testnet finality activation:

1. **Zero divergent beacon selections** for 30 consecutive days across all nodes.
2. **Zero IBD replay diffs** for 30 consecutive days.
3. **Stale-coupling incidence** below 1% of epochs.
4. **Paired-header lag p95** below threshold above on all nodes.
5. **No bugs marked Critical or High** in the issue tracker against the DACE codebase.

If any criterion is missed, extend Phase 2a until the gating window is satisfied.

## Failure handling

- If divergent beacons are observed: immediately collect node logs, paired-chain header snapshots, and consensus param dumps. Investigate before any further activation step.
- If IBD diffs appear: a node operator with the divergent fork shares their `dace_dump_state.json` (new RPC for shadow mode). Compare against canonical via diff harness in [test/functional/dace_ibd_diff_harness.py](../../test/functional/) (Phase 2 addition).

## Wallet & UX integration

The desktop wallet at [verium/desktop/verium-app/](../../../verium/desktop/verium-app/) must surface:

- DACE status panel: `binarychain_status` fields.
- Beacon health: current epoch's beacon hash, height, depth.
- Paired-header lag p95/p99 chart.
- Active anchor: hash, epoch.

This is the first piece of user-visible DACE UX. Implementation should land in the same PR as Phase 2a activation flag.

## Decision log

Maintain a running decision log of:
- Operator hardware changes.
- Constant tweaks (testnet `-staleemaxepochs` etc.).
- Any anomalies and how they were resolved.

This log feeds into the Phase 2c audit packet.
