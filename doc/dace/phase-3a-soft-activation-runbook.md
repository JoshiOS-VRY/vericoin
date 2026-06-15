# Phase 3a — Mainnet Soft Activation Runbook

Status: Draft
Phase: 3a (mainnet soft activation)
Duration: 30 days
Prerequisite: Phase 2c audit + bounty clean ([phase-2c-audit-charter.md](phase-2c-audit-charter.md))

## Objective

Activate DACE rules on mainnet in **soft mode**:

- Claim redemption is **enabled**.
- Paired-anchor refs (`pairedAnchorRef` in headers) are **advisory** — invalid values do not invalidate blocks; they produce warning logs only.
- All other DACE infrastructure (ticket registry, committee sortition, beacon selection, JA lifecycle, reward accumulator) is fully active.

Goal: prove the implementation handles mainnet conditions for 30 days before flipping the hard switch.

## Configuration

Tagged release with consensus rules compiled in and inert. Daemon CLI flags:

```
# Pre-tagged release — daemons consult chainparams for activation height.
# No CLI override should be necessary on mainnet.
```

`chainparams.cpp` should set `BinaryChainHeightVRC` and `BinaryChainHeightVRM` to the mainnet-frozen activation heights. Activation timestamp target: `T*` from Section 14.1 of the design guide.

Soft-mode flag in chainparams:

```cpp
consensus.DaceSoftActivationOnly = true;   // Phase 3a
```

When true:
- Block validity does not reject on `pairedAnchorRef` mismatch.
- A warning is logged: `"BC soft: block #N has invalid pairedAnchorRef, ignored"`.
- All other DACE checks (kernel coupling, reward accumulator, claim redemption) are fully enforced.

## Sequence

| Day | Event |
|-----|-------|
| T-120 | Tagged release with inert activation, full BIP9-style signaling. Public announcement, exchanges notified. |
| T-60 | Signaling threshold (BIP9 80%) targeted. If under threshold, defer activation by 60 days. |
| T-14 | Wallets warn loudly on launch if upgrade not applied. Pool operators contacted directly. |
| T-0 | Activation heights reached on both chains. Soft mode begins. |
| T+1 to T+30 | Active soft-mode period. Daily metrics review. |
| T+30 | Decision point: proceed to Phase 3b (hard) or extend Phase 3a. |

## Daily monitoring

Operators of the official seed/observer nodes publish daily:

- Output of `binarychain_status` and `binarychain_metrics` on at least 5 mainnet nodes.
- Soft-warning rate: count of `"BC soft:"` log entries per 1000 blocks.
- Stale-coupling incidence (% of epochs).

## Gating criteria for Phase 3b (hard activation)

To proceed to mandatory enforcement:

1. **Zero consensus diffs** during nightly replay-from-genesis on a clean node, across all 30 days.
2. **Soft-warning rate** < 0.1% of blocks (i.e., honest miners and stakers are constructing valid `pairedAnchorRef`).
3. **Stale-coupling incidence** < 5% of epochs.
4. **Active anchor cadence**: certified anchors land within `FinalityWindowSeconds` for ≥ 80% of epochs.
5. **No Critical** issues in the public issue tracker.

If criteria fail, extend Phase 3a by 30 days and reassess. If they fail again, return to Phase 2b for the affected area.

## Replay-from-genesis procedure

Each night, on a clean node:

```
$ rm -rf /tmp/dace-replay && mkdir -p /tmp/dace-replay
$ veriumd  -datadir=/tmp/dace-replay/vrm -daemon=0 \
           -dbcache=4096 -checkblockindex=1 -reindex 2>&1 \
           | tee /tmp/dace-replay/vrm-replay.log

$ vericoind -datadir=/tmp/dace-replay/vrc -daemon=0 \
            -dbcache=4096 -checkblockindex=1 -reindex 2>&1 \
            | tee /tmp/dace-replay/vrc-replay.log
```

Confirm:
- Final tip hash matches the live mainnet tip on both chains.
- `binarychain_metrics.ibd_replay_consensus_diffs` is 0.

## Rollback plan

Soft mode is the rollback. If a Critical issue is discovered during Phase 3a:

1. Tagged emergency release that sets `DaceSoftActivationOnly = true` indefinitely (already the Phase 3a default — no consensus action needed).
2. Critical fixes go through abbreviated 14-day audit re-review before Phase 3b is rescheduled.
3. Hard activation height is bumped to a date at least 60 days out.

## Decision log

Maintain a per-day log of any:
- Soft-warning rate spikes.
- Stale-coupling incidents.
- Bug reports.
- Operator complaints.

The log + the public metrics archive feed into the Phase 3b go/no-go decision.
