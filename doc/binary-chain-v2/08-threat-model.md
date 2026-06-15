# Binary Chain v2 — Threat Model and Attack Analysis

> Spans all layers. Honest framing: no attack is "impossible." We use "cost increases,"
> "requires collusion," and "bounded by X." Each entry follows
> Attack / Prerequisites / Impact / Mitigation / Residual risk / Test case needed.

## Conventions

- **VRM** = Verium PoWT chain; **VRC** = VeriCoin PoST chain.
- "Activated" / `JA_active` per [`01-dace-core.md`](01-dace-core.md).
- Degradation ladder per [`02-coupled-degradation-biced.md`](02-coupled-degradation-biced.md).
- Jackpot is app-layer per [`05-binary-jackpot-rewards.md`](05-binary-jackpot-rewards.md).

---

## 1. 51% on VRM only

- **Attack:** Majority Verium hashpower rewrites VRM history / withholds or biases beacons.
- **Prerequisites:** >50% sustained VRM scrypt² hashpower.
- **Impact:** Can reorg VRM below `K_BEACON`; can attempt to influence which block becomes a
  beacon candidate within the bounded fallback window. Cannot forge a VeriCoin committee
  certificate, so cannot unilaterally activate a Joint Anchor.
- **Mitigation:** Beacons usable only when `K_BEACON` (50) deep on most-work chain;
  activation also requires the VRC committee quorum + coinbase inclusion + epoch delay; beacon
  ratchets once activated. Reorg above buried depth cannot change an activated beacon.
- **Residual risk:** Bounded beacon-selection bias within the fallback ladder; temporary
  stale-coupling/degradation if VRM is unstable. Bounded by `K_BEACON` and `BeaconFallbackWindow`.
- **Test:** Simulate >50% VRM with beacon-grinding; assert no divergent activated beacon and
  no unilateral activation.

## 2. 51% on VRC only

- **Attack:** Majority VeriCoin stake/committee power signs malicious anchors.
- **Prerequisites:** Control ≥ `q` (2/3) of selected committee ticket weight, sustained.
- **Impact:** Could certify a malicious JA, but it still needs a `K_BEACON`-deep VRM beacon
  and coinbase inclusion + delay to activate; equivocation is slashable.
- **Mitigation:** Bonded `TicketStakeUnit` locks; sortition without replacement; slashing of
  equivocation/conflicting recovery; 2/3 quorum; delayed activation gives time to detect.
- **Residual risk:** A true ≥2/3 honest-majority violation breaks the security assumption
  (as in any BFT system); cost = acquiring and bonding ≥2/3 of active stake and risking slash.
- **Test:** Equivocation drill: two JAs same epoch → slash; assert stake burned, ticket removed.

## 3. Combined 51% (VRM + VRC)

- **Attack:** Attacker controls majority of both chains simultaneously.
- **Prerequisites:** >50% VRM hashpower **and** ≥2/3 VRC committee weight at once.
- **Impact:** Can produce and activate malicious anchors; this is the worst case.
- **Mitigation:** Economic cost of simultaneously dominating two sovereign resource bases;
  delayed activation + slashing raise cost; recovery anchor and stale-coupling limit
  follow-on damage; emergency social response (doc 09 rollback caveats).
- **Residual risk:** Real and unbounded if both majorities are achieved — documented, not
  denied. Coupling **increases** the combined cost vs attacking either chain alone.
- **Test:** Combined-majority simulation to quantify cost and confirm slashing still applies.

## 4. Stake grinding

- **Attack:** Grind stake/identities to bias committee sortition or seed.
- **Prerequisites:** Ability to create/split tickets or influence `S_e` inputs.
- **Impact:** Skewed committee selection.
- **Mitigation:** Fixed-size tickets (splitting yields linear, not superlinear influence);
  sortition seed `S_e = H(JA_{e-1} || beacon_e || VRC_checkpoint_{e-1})` uses buried,
  committed inputs; without-replacement sampling.
- **Residual risk:** Marginal bias proportional to bonded share; bounded and monitored
  (`top10_ticket_share`, `seat_to_bonded_ratio_top10`).
- **Test:** Seat-share vs bonded-share regression over many epochs; assert near-linear.

## 5. Long-range PoS attack

- **Attack:** Rewrite deep VRC history from an old checkpoint using old keys.
- **Prerequisites:** Old bonded keys / cheap historical stake.
- **Impact:** Alternate VRC history could fabricate certificates.
- **Mitigation:** Bootstrap anchored to `DACE_BOOTSTRAP_*` in chainparams; bonded tickets with
  lockup/unbond windows and slashing; activated anchors ratchet and are coinbase-buried on VRM
  (objective work backs the timeline); weak-subjectivity checkpoints.
- **Residual risk:** Standard PoS weak-subjectivity assumption for new/long-offline nodes.
- **Test:** Replay-from-bootstrap; attempt alternate-history certificate; assert rejection.

## 6. Anchor withholding

- **Attack:** Committee certifies but withholds the anchor/signatures to stall activation.
- **Prerequisites:** Enough committee weight to deny quorum, or selective release.
- **Impact:** Missed anchors → degradation; no base halt.
- **Mitigation:** `FinalityGraceBlocks` inclusion deadline; `jasig` gossip during assembly;
  `S_MAX` → recovery anchor; degradation only throttles bonus.
- **Residual risk:** Temporary bonus degradation; bounded by `S_MAX` then recovery.
- **Test:** Withhold drill; assert ladder progression and recovery anchor activation.

## 7. Header withholding (foreign)

- **Attack:** Withhold the other chain's headers from a node/region.
- **Prerequisites:** Control of a victim's foreign-header feed.
- **Impact:** Victim cannot advance couplable frontier → stale-coupling.
- **Mitigation:** Multi-peer `getxheaders` (≥4 peers), peer diversity, bounded-ahead pacing,
  stale-coupling (continue base, pause finality upgrades).
- **Residual risk:** Localized degradation while eclipsed; no acceptance of invalid data.
- **Test:** Single-source feed cannot advance frontier; multi-peer recovers.

## 8. Eclipse attack

- **Attack:** Surround a node with malicious peers on its own and/or foreign feed.
- **Prerequisites:** Control of victim's peer connections.
- **Impact:** Feed stale/biased view; attempt to make victim accept attacker's anchor.
- **Mitigation:** Netgroup/ASN diversity for paired feed; require agreement on buried prefix;
  anchored bootstrap; activated-only validity (cannot be tricked into using unburied beacon).
- **Residual risk:** Victim may be stalled/degraded while eclipsed; bounded, never invalid-accept.
- **Test:** Eclipse simulation; assert no invalid activation, only degradation.

## 9. Stale-header griefing

- **Attack:** Flood valid-but-stale foreign headers to waste resources / stall frontier.
- **Prerequisites:** Peer connections; valid old headers.
- **Impact:** Resource use; no validity effect.
- **Mitigation:** Bounded-ahead refusal (`CoupleLookaheadEpochs`), rate-limit scoring
  (`getxheaders` spam = 10/excess), store-but-don't-advance semantics.
- **Residual risk:** Minor bandwidth/CPU; bounded by scoring/disconnect.
- **Test:** Flood test; assert frontier unchanged and peer scored/disconnected.

## 10. Pool-address evasion (jackpot)

- **Attack:** A pool/exchange rotates to fresh addresses to qualify for the individual reward.
- **Prerequisites:** Ability to create fresh payout addresses (trivial).
- **Impact:** An excluded entity wins the individual reward.
- **Mitigation:** **App-layer** exclusion registry + clustering heuristics + Binary Identity
  registration + minimum useful-contribution thresholds; published, appealable methodology.
- **Residual risk:** Real — classification is subjective and evadable; this is **why exclusion
  is app-layer policy, never consensus** (doc 12). Bounded by detection + post-hoc correction.
- **Test:** Red-team address rotation against the exclusion heuristics.

## 11. Jackpot Sybil farming

- **Attack:** Split holdings into many small identities to multiply base tickets.
- **Prerequisites:** Capital to distribute; ability to register many identities.
- **Impact:** Inflated odds for the attacker.
- **Mitigation:** `tickets = 1 + floor(sqrt(units))` flattening; registration friction
  (Binary Identity); minimum-contribution threshold; stake-time/work weighting not address count.
- **Residual risk:** Genuine distributed work/stake gains a bounded edge; documented.
- **Test:** Sybil simulation comparing split vs unified expected payout.

## 12. Jackpot seed manipulation

- **Attack:** Bias `jackpot_seed` by influencing beacon/certificate at the boundary.
- **Prerequisites:** Ability to grind beacon or certificate inputs near the draw.
- **Impact:** Influence winner selection.
- **Mitigation:** Seed uses **buried/finalized** inputs (`K_BEACON`-deep beacon window root,
  committed certificate root, previous seed, epoch id); selection is off-chain so it cannot be
  retried by re-mining a single block; published recomputable procedure.
- **Residual risk:** Same bounded beacon-bias as attack #1, now applied to an off-chain draw;
  bounded by burial depth + sqrt weighting.
- **Test:** Attempt seed grinding within fallback window; quantify achievable bias.

## 13. Sponsor / treasury abuse

- **Attack:** A sponsor/foundation insider funds then captures the pool, or self-deals.
- **Prerequisites:** Control of promotion treasury keys / process.
- **Impact:** Misappropriated pool; rigged event.
- **Mitigation:** Multisig treasury; published deposits/rollover on-chain; exclusion of
  operator/aggregation addresses; recomputable winner; external audit of the promotion process.
- **Residual risk:** Governance/insider risk inherent to any foundation-run promotion; bounded
  by transparency + multisig + audit. **Not a consensus property.**
- **Test:** Process audit + reconciliation of on-chain deposits vs declared pool.

## 14. Reward-claim replay

- **Attack:** Redeem the same claim twice (replay the Merkle proof).
- **Prerequisites:** A valid claim witness.
- **Impact:** Double payout from escrow.
- **Mitigation:** `ClaimSpentSet` marks `(epoch, recipient_pkh, source_chain)` spent on first
  redemption; second attempt fails `ValidateClaim`.
- **Residual risk:** Low if spent-set is consensus state and persisted; depends on completing
  the (currently unwired) enforcement.
- **Test:** Redeem twice; assert second rejected; persistence across restart.

## 15. Unclaimed-reward abuse

- **Attack:** Game expiry/recycling to redirect or re-mint expired escrow.
- **Prerequisites:** Influence over expiry destination.
- **Impact:** Diverted expired funds.
- **Mitigation:** Expiry destination is the **existing** burn/treasury rule (`ClaimExpiryEpochs`);
  v2 adds **no new** consensus destination; jackpot funding from treasury is app-layer policy.
- **Residual risk:** Treasury governance risk only; no consensus diversion path exists.
- **Test:** Expire claims; assert funds go only to the pre-existing destination.

## 16. Reorgs around anchor activation

- **Attack:** Reorg VRM/VRC right at the activation boundary to flip `JA_active`.
- **Prerequisites:** Reorg capability near the boundary.
- **Impact:** Disagreement on the active anchor.
- **Mitigation:** Activation requires epoch delay + 6 VRM confs; `AnchorLifecycle::Rewind`
  deterministically demotes anchors whose preconditions fail; activated-only validity; beacon
  ratchet survives shallow reorgs.
- **Residual risk:** Brief ambiguity within the confirmation window; bounded by `BEACON_EPOCH_VRM_CONF`.
- **Test:** Reorg-at-activation drill on both chains; assert deterministic convergence.

## 17. Chain stalls / liveness failures

- **Attack/Event:** Committee outage, partition, or bug stalls certification.
- **Prerequisites:** Loss of quorum or connectivity.
- **Impact:** Missed anchors → degradation; base chains continue.
- **Mitigation:** Stale-coupling (base continues), degradation ladder (bonus only), `S_MAX` →
  recovery anchor with `RecoveryThreshold` (4/5).
- **Residual risk:** Extended bonus degradation; bounded by recovery path.
- **Test:** 33%-offline and partition drills; assert recovery after churn.

## 18. Network partitions

- **Attack/Event:** The two chains' networks partition from each other.
- **Prerequisites:** Network split.
- **Impact:** Each side sees missed anchors; both degrade bonus.
- **Mitigation:** Activated-only validity prevents divergent validity; on heal, deterministic
  beacon/anchor selection reconciles; recovery anchor if prolonged.
- **Residual risk:** Bonus degradation during partition; base unaffected.
- **Test:** Partition-and-heal simulation; assert identical post-heal state across nodes.

## 19. Emergency checkpoint abuse

- **Attack:** Misuse of emergency checkpoints / recovery anchor as a governance backdoor.
- **Prerequisites:** Control of checkpoint/recovery process or enough recovery weight.
- **Impact:** Force a chain view / censor.
- **Mitigation:** No irreversible kill switch by design; recovery anchor needs `RecoveryThreshold`
  (4/5) bonded weight + `K_BEACON`-deep beacon and is slashable if conflicting; recovery only
  after `S_MAX`; all actions on-chain and auditable.
- **Residual risk:** A 4/5 bonded supermajority is powerful; documented as a known trust
  assumption, bounded by slashing + transparency.
- **Test:** Conflicting-recovery drill → slash; assert single deterministic recovery outcome.

---

## Cross-cutting measurements (wire to `binarychain_metrics`)

`divergent_beacon_selections` (must stay 0), `reorgs_at_beacon`, `committee_missed_votes`,
`foreign_payee_rejections` (must stay 0), `consecutive_missed_anchors`, `stale_coupling_entries`,
`recovery_anchors_built/activated`, `claim_redemptions_total/rejected`, `slashes_total`,
`top10_ticket_share`, `seat_to_bonded_ratio_top10`, `ibd_replay_consensus_diffs` (must stay 0).

## Honesty statement

This model does not claim any attack is impossible. It claims that coupling **increases the
cost** of attacking the ecosystem versus attacking either chain alone, that failures are
**bounded** (degraded bonus, not base halt or invalid-accept), and that the highest-risk
surfaces (combined 51%, exclusion evasion, sponsor abuse) are explicitly acknowledged rather
than hand-waved.
