# Phase 2b — Testnet Finality Runbook

Status: Draft
Phase: 2b (testnet active finality)
Duration: ≥ 90 days
Prerequisite: Phase 2a sign-off ([shadow-mode runbook](phase-2a-shadow-mode-runbook.md))

## Objective

Activate the full DACE consensus rules on testnet:
- Ticket registry, committee sortition, Joint Anchor certification (with quorum signatures and slashing).
- Reward accumulator + claims redemption (claims enabled).
- Cross-chain commitments enforced (block validity gated on `pairedAnchorRef`, `beaconRef`, `rewardAccumulatorRoot`).

Goal: prove that the full design works under adversarial conditions before mainnet.

## Configuration

Drop the `-dace-shadow-mode` flag from Phase 2a configurations. Set:

```
-binarychainheight=<testnet hard activation height>
-dace-shadow-mode=0
```

## Required adversarial drills

Conduct each drill within the 90-day window. Drills must pass without intervention from devs.

| Drill | Procedure | Pass criterion |
|-------|-----------|------------------|
| **Partition drill** | Network-partition VRM mining into two halves for 6 hours. Resume connection. | Both halves resync, beacons re-deterministic, no permanent fork. Active anchor advances after partition heals. |
| **33% offline drill** | Stop one-third of bonded ticket operators for 24 hours. | Chain continues (quorum still met); committee sortition adapts; no slashing. |
| **Stale-coupling drill** | Block all paired-header relay between VRM and VRC nodes for 6 epochs. | VRC enters stale-coupling. Finality stops advancing, but local processing continues. After unblock, finality resumes; no false slashing. |
| **Recovery drill** | Take 50% of bonded ticket operators offline (just below quorum) for `S_MAX` epochs. Then bring a recovery quorum (80%) online. | RecoveryEligible warning surfaces. Recovery anchor builds and activates. Finality resumes from a fresh anchor. |
| **Equivocation drill** | Have one operator sign two distinct anchors for the same epoch. | Slashing evidence is detected. Offender's stake burns; ticket is removed in the next epoch. |
| **Replay-from-genesis** | Run an isolated node from genesis using only public testnet data. | Final state matches the canonical testnet tip. No consensus diffs. |

## Wallet integration

Desktop wallet [verium/desktop/verium-app/](../../../verium/desktop/verium-app/) additions in Phase 2b:

- New page **Binary Chain** under the sidebar.
- Status panel reading `binarychain_status` and `binarychain_metrics` RPCs.
- **Claim balances** view: lists redeemable claim leaves with one-click redemption.
- **Beacon health** view: current epoch's beacon, depth, confirmations.
- **Finality status**: active anchor, last certification time, stale-coupling state.
- **Committee participation** view (for ticket operators): bonded stake, expected next selection, signing performance.

Tauri command additions in [verium/desktop/verium-app/src-tauri/src/commands.rs](../../../verium/desktop/verium-app/src-tauri/src/commands.rs):

- `binarychain_status` (Tauri wrapper around node RPC)
- `binarychain_metrics`
- `binarychain_anchor`
- `binarychain_redeem_claim(leaf_hash)` (constructs and signs redemption tx)
- `binarychain_register_ticket(stake_outpoint, operator_pubkey)`
- `binarychain_unbond_ticket(ticket_id)`

## Public participation

- Public-facing testnet faucet with VRC and VRM available.
- Documented onboarding for at least 10 independent ticket operators, geographically distributed.
- Public mining pool support for VRM testnet so non-developer miners can participate.

## Gating criteria for Phase 2c

To proceed to external audit:

1. All adversarial drills passed on first attempt or after fix + re-run.
2. **Zero unexpected slashings** (only equivocation-drill slashes counted as expected).
3. **Finality cadence**: certified anchors land within `FinalityWindowSeconds` for 90% of epochs.
4. **Claim redemption success rate**: ≥ 99% (excluding intentional adversarial attempts).
5. **Replay-from-genesis** clean across at least 3 independent fresh nodes.

## Metrics tracking

Daily `binarychain_metrics` dump posted to a public status page. Visible to the community and to the audit team.
