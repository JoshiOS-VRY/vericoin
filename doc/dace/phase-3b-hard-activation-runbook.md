# Phase 3b — Mainnet Hard Activation Runbook

Status: Draft
Phase: 3b (mainnet mandatory enforcement)
Duration: indefinite (DACE is mainnet from here)
Prerequisite: Phase 3a clean run for 30 days ([phase-3a-soft-activation-runbook.md](phase-3a-soft-activation-runbook.md))

## Objective

Flip DACE from soft (advisory) to **mandatory** consensus enforcement on mainnet:

- `pairedAnchorRef` mismatches **reject** blocks.
- All other DACE rules already mandatory from Phase 3a.

Effectively this is the "real" mainnet activation. From this point forward, the chains are consensus-coupled.

## Configuration

Tagged release with:

```cpp
consensus.DaceSoftActivationOnly = false;   // Phase 3b
```

All other DACE chainparams unchanged.

## T-120/T-60/T-14/T-0/T+7 sequence

| Day | Event | Owners |
|-----|-------|--------|
| **T-120** | Tagged 3b release; public announcement; exchanges and pool operators notified. BIP9 signaling target window opens. | Core devs |
| **T-60** | BIP9 80% threshold check. If not met, defer T-0 by 60 days and extend Phase 3a. | Core devs |
| **T-30** | Final operator outreach. Wallet update prompts on launch. Status-page banner: "DACE hard activation in 30 days." | Core devs + community |
| **T-14** | Pre-activation system check. Verify Phase 3a metrics are still green. Verify at least one ticket operator in each major timezone is online and signing. | Core devs + community |
| **T-7** | Final go/no-go meeting. Signed off by core devs, exchange contacts, top pool ops, and audit firm liaison. | Governance |
| **T-0** | Hard activation height reached on both chains. Blocks with invalid `pairedAnchorRef` are now rejected. | Network |
| **T+1 to T+7** | Active hard-enforcement period. Daily metrics review by core devs. Status page updated hourly. | Core devs |
| **T+7** | `BC_PROTOCOL_VERSION` enforcement: peers below the version are refused. | Network |
| **T+30** | Post-activation review meeting. Activation declared successful or rollback triggered. | Governance |
| **T+90** | Legacy RPC fields that reference `nFlags` may be deprecated. Phase 4 work begins. | Core devs |

## Monitoring

24/7 monitoring for the first 7 days post-activation. On-call rotation across core devs.

Key metrics (read continuously from `binarychain_metrics`):

| Metric | Alert threshold |
|--------|------------------|
| `divergent_beacon_selections` | Any non-zero increment over 1h |
| `ibd_replay_consensus_diffs` | Any non-zero |
| `foreign_payee_rejections` | > 0.5% of blocks per hour |
| `consecutive_missed_anchors` | > `S_GRACE` epochs |
| `stale_coupling_entries` | > 1 per node per hour |
| `slashes_total` | Any unexpected slash |

Each alert page routes to the on-call dev with a 5-minute response SLA during the first 7 days.

## Rollback plan

If a Critical issue is discovered:

1. **Emergency consensus rollback** is **not available** for the hard activation by design. The chain is committed.
2. **Mitigations**: a new chainparams release can extend `S_MAX` (allowing slower recovery anchors), tighten `K_BEACON` (more confirmations required), or temporarily disable claim redemption (escrow funds remain safe but locked).
3. **Worst case**: if the issue requires consensus-rule changes, prepare a follow-up hard fork. The recovery anchor mechanism is the protocol-native escape hatch for severe stalls; use it before any emergency fork.

Document the rollback decision in the public status page within 1 hour.

## Sign-off requirements

Per T-7: written sign-off from:

- All core devs.
- Audit firm contact (clean-pass reaffirmation).
- Top 5 exchanges by VRC/VRM volume.
- Top 5 mining pool operators.
- Top 5 ticket operators by bonded stake.

Sign-offs are public.

## Post-activation publication

After T+30:

- Publish the activation retrospective.
- Update [DACE-0](DACE-0-overview-and-constants.md) status from "Draft" to "Active mainnet".
- Tag all DACE-1 through DACE-7 as "Active mainnet".
- Commit `DACE_BOOTSTRAP_VRM_HEADER`, `DACE_BOOTSTRAP_VRC_HEADER`, `DACE_BOOTSTRAP_JA_GENESIS` in chainparams as the canonical bootstrap anchors for fresh nodes.

This activation is the formal birth of the Binary Chain on mainnet.
