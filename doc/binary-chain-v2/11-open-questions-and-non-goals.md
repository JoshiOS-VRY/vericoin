# Binary Chain v2 — Open Questions and Non-Goals

## Part A — Open questions (decide at Phase 0)

### Consensus / DACE

1. **Degradation parameters.** σ/φ reduction factor at `BONUS_ACCRUAL_REDUCED` (proposed
   0.5) and recovery window `W` (proposed 3 healthy anchors). Confirm these are the right
   defaults and that they are pure functions of canonical `consecutive_missed_anchors`.
2. **Activation/bootstrap heights.** `BinaryChainHeightVRC`, `BinaryChainHeightVRM`, and the
   three `DaceBootstrap*` hashes — must be frozen ≥120 days before activation and coordinated
   across both chains.
3. **Verium-side DACE scope.** What is the minimum viable `verium/src/dace/` for mainnet
   (beacon production + extended header + coinbase JA inclusion + VeriCoin-header ingestion)?
   How much of the VeriCoin module can be shared vs forked?
4. **Committee certificate verification on the Verium side.** For v2, is it acceptable that
   Verium consumes the *activated* JA via coinbase inclusion rather than independently
   verifying the VeriCoin committee certificate (doc 03 §6)? Recommended: yes for v2.
5. **Expired-claim destination.** Confirm the existing burn/treasury destination for
   `ClaimExpiryEpochs` and that v2 reads from it without adding a new consensus destination.
6. **Signature scheme.** Stay on ECDSA per-signer lists for Phase 1; schedule musig2/Schnorr
   aggregation for Phase 4?

### Reward / economics

7. **σ/φ values.** Keep 4% / 10%, or recalibrate after testnet data?
8. **Reduction vs market asymmetry.** The σ/φ split transfers asymmetric value depending on
   VRM/VRC relative price. Accept the asymmetry (no consensus oracle) and adjust bands by
   governance/hard fork if needed?

### Jackpot (app-layer)

9. **Epoch length.** Confirm 90 days default (vs 30/180). Separate annual "grand" event?
10. **Split.** Confirm 85% winner / 15% rollover.
11. **Funding mix.** Target ratio of sponsor : rollover : expired-recycle : bonus-skim : fees?
12. **Ticket cap.** Add a `TICKET_CAP` on top of `1 + floor(sqrt(units))`? At what value?
13. **Binary Identity requirement.** Mandatory registration to be eligible, or optional with
    higher weight for registered participants?
14. **Exclusion methodology + appeals.** Who maintains the registry, how are clusters
    detected, and what is the correction process?
15. **Jurisdiction strategy.** Which jurisdictions are in/out at launch; geofencing approach;
    tax-reporting thresholds; no-purchase-necessary free-entry mechanism.

### Binary Address (wallet)

16. **Payload encoding.** Option A (address/hash-commit, recommended) vs Option B
    (pubkey-commit)?
17. **SLIP-44 coin types** for VRC and VRM derivation paths — confirm registered values.
18. **Per-use vs static** Binary Address default in the wallet (privacy vs convenience).

### Cross-cutting

19. **Naming rollout.** How/when to footnote or rename the "Binary Chain v3" labels in
    `doc/dace/` so only "Binary Chain v2" + "DACE engine" remain public.
20. **Monitoring SLOs.** Target values for `binarychain_metrics` counters that gate phase
    transitions (e.g. certified-in-window %, stale-coupling %, divergent beacons = 0).

## Part B — Things that should explicitly NOT be done

> If a future change proposes any of these, it must be rejected unless it goes through the
> doc 12 promotion criteria and a full BIP/audit cycle.

1. Do **not** put jackpot funding bps, exclusion lists, USD valuation, sponsor rules, or
   winner selection into `consensus/params.h` or any consensus path in v2.
2. Do **not** place jackpot eligibility logic inside `vericoin/src/dace/`.
3. Do **not** make pool/exchange/custodial exclusion a consensus validity rule (it is
   subjective, addresses rotate — it is governance).
4. Do **not** add a USD oracle to consensus. USD is display-only.
5. Do **not** require specialized anchor hardware, or add SNARK/VDF/proof generation to v1.
6. Do **not** require a live foreign-chain payee in any block (rewards are delayed pull claims).
7. Do **not** use the word "lottery" (or "raffle"/"gambling") in protocol docs, wallet UI, or
   marketing.
8. Do **not** allow pool/exchange/custodial addresses to win the individual major reward.
9. Do **not** rename the project to DACE or BiCED; keep "Binary Chain" as the public identity.
10. Do **not** let "DACE v3" and "Binary Chain v2" coexist casually.
11. Do **not** add per-block BiCED TLV coupling or any second commitment layer competing with
    the DACE header fields.
12. Do **not** let degradation reduce base PoWT subsidy or base PoST staking rewards — only
    bonus σ/φ flows degrade.
13. Do **not** add a new consensus money destination to fund the jackpot (read the existing
    expired-claim destination at the app layer instead).
14. Do **not** let any node-local freshness signal (peer lag, header availability, gossip
    timing, dashboard health) influence σ/φ accumulator roots — warnings/metrics only.
15. Do **not** treat a header-only "cumulative trust" number as valid for the PoST side.
16. Do **not** overclaim ASIC-proofness, legal safety, or attack impossibility anywhere.
17. Do **not** introduce an irreversible kill switch; rely on bounded stale-coupling +
    recovery anchor.
