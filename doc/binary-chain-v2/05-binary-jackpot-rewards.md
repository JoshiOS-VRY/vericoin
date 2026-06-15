# Binary Chain v2 — Binary Jackpot Rewards

> **App-layer / foundation promotion (layer 5) + explorer/indexer (layer 4) for v1.**
> No jackpot logic, constants, exclusion lists, USD valuation, sponsor rules, or winner
> selection live in consensus for v2. See [`12-consensus-boundary.md`](12-consensus-boundary.md).
> This is **not a lottery**; see the compliance section before using any of this language
> publicly.

## 1. Purpose

Encourage normal users to download the wallet, mine Verium, stake VeriCoin, run nodes, and
contribute to Binary Chain health by making one rare, large, auditable **Binary Chain
Reward Event** possible roughly once per long epoch.

Approved terminology (never "lottery"):

- Binary Jackpot Rewards
- Jackpot Epoch
- Participation Prize Epoch
- Major Participation Reward Event
- Binary Chain Reward Event

## 2. Why app-layer first

Running v1 as a foundation-operated promotion that **reads on-chain participation proofs**:

- removes consensus risk (no subjective rules, no USD, no money destination in consensus);
- removes the legal risk of encoding a prize draw into the protocol;
- lets parameters (epoch length, split, exclusions) change without a hard fork;
- still produces a fully auditable, deterministic result because the inputs are on-chain.

Protocol-native jackpot claims are a **Phase 4** consideration only (doc 09), and only if
they can be made fully deterministic with no subjective input.

## 3. Event cadence (epoch length analysis)

Target: one major event per long epoch, triggered by a successful Joint Anchor activation
at the epoch boundary. If the chain is degraded or no anchor activates, the pool **rolls
forward** to the next epoch.

| Epoch length | Pros | Cons | Verdict |
|---|---|---|---|
| 30 days | Frequent excitement; faster feedback | Smaller pools; more operational/compliance cycles; more Sybil cycles | Too frequent for a "mega" event |
| **90 days (default)** | Pools accumulate to meaningful size; quarterly rhythm matches treasury/sponsor cycles; fewer compliance events | Less frequent dopamine | **Recommended default** |
| 180 days | Largest pools | Too rare to drive ongoing engagement; long lock of rolled funds | Reserve for a separate annual "grand" event |

**Default: 90-day Jackpot Epoch.** One eligible **individual** winner per event.

## 4. Pool growth (how the pool is funded)

Funding sources, in priority order (none meaningfully taxes base rewards):

1. **Sponsor / foundation / treasury deposits** — ordinary on-chain transactions to a
   promotion-controlled address. Primary source early on.
2. **Rollover** from previous events (e.g. the 15% retained share).
3. **Expired / unclaimed Binary Chain rewards** — funded from the treasury that *already*
   receives expired escrow (`ClaimExpiryEpochs`), as a foundation policy. **No new
   consensus destination** (doc 12 guard #1).
4. **Tiny share of cross-chain bonus rewards** — a few bps skimmed at the app/treasury
   layer from σ/φ flows the foundation controls. Not a consensus rule.
5. **Optional app/ecosystem revenue** — explorer/app monetization, partnerships.
6. **Optional tiny share of transaction fees** — only if community-approved; kept small.

> Do not fund the pool primarily by taking meaningful base rewards away from miners/stakers.

### 4.1 Coin-denominated pool vs displayed USD

```text
CONSENSUS / AUDIT TRUTH:  pool is denominated in VRM and VRC amounts.
DISPLAY ONLY:             wallet/explorer may show an estimated USD value.
```

There is **no USD oracle in consensus**. USD is a display convenience computed by the
explorer/app from market data, clearly labeled "estimated."

### 4.2 Example pool (illustrative assumptions)

```text
Assume VRC = $2, VRM = $25 (illustrative; not a price claim).
Target mega pool          = $250,000
Example composition       = 75,000 VRC ($150,000) + 4,000 VRM ($100,000) = $250,000
Winner payout             = 85%  -> ~$212,500 (63,750 VRC + 3,400 VRM)
Rollover                  = 15%  -> ~$37,500   (11,250 VRC +   600 VRM)
```

Payouts are **native** VRC and VRM to the winner's two native addresses (one Binary
Address; doc 06). Not wrapped, not a bridge.

### 4.3 Reaching pool milestones

Assuming a 90-day epoch and the illustrative prices above:

| Target | Plausible composition | How it gets there |
|--------|----------------------|-------------------|
| $10,000 | 4,000 VRC + 80 VRM | Single modest sponsor + first-epoch skim + rollover seed |
| $25,000 | 9,000 VRC + 280 VRM | Sponsor + 90 days of small bonus skim + expired-claim recycling |
| $100,000 | 35,000 VRC + 1,200 VRM | Foundation matching + steady skim + accumulated rollover |
| $250,000+ | 75,000 VRC + 4,000 VRM | Foundation + multiple sponsors + sustained skim + rollover |

### 4.4 Scenario: 10,000 miners + 50,000 stakers

```text
Participants            = 60,000 (10,000 VRM miners + 50,000 VRC stakers)
Quarterly event target  = $250,000
Per-month pool need      = $250,000 / 3 = $83,333
Per-participant per-month = $83,333 / 60,000 = ~$1.39
```

So a $250k quarterly event is supported if the ecosystem can route **~$1.39 of value per
participant per month** into the pool. The design intent is that **most of this comes from
sponsors/treasury/rollover/skim, not from participants' own base rewards.** For intuition:
if only the bonus skim funded it, and the σ/φ bonus pools were worth, say, $40k/month
ecosystem-wide, a ~25% foundation-policy skim would cover ~$10k/month, with sponsors and
rollover covering the rest. The point of the table is to size sponsorship, not to tax
miners.

## 5. Winner selection

The Joint Anchor activation at the epoch boundary is the **draw trigger**, but the winner
is **not** simply whoever mined/staked the exact anchor block (that would reward a single
lucky block, invite grinding, and concentrate odds).

```text
At the end of a successful Jackpot Epoch (indexer/app, layer 4/5):
  1. Build eligible individual participant set from on-chain participation proofs.
  2. Exclude known pool, exchange, custodial, and operator-aggregation addresses
     (app-layer promotional policy; NOT a consensus rule).
  3. Compute contribution scores (useful work mined / stake-time certified / node uptime
     proven via participation receipts).
  4. Apply anti-whale weighting.
  5. Generate the deterministic seed from both chains' finalized state.
  6. Select one winner.
```

### 5.1 Deterministic, auditable seed (not consensus randomness)

```text
jackpot_seed = Hash(
    verium_powt_beacon_window_root        // from the activating epoch's beacon window
 || vericoin_post_certificate_root        // committee_root / VRC checkpoint of the anchor
 || previous_jackpot_seed
 || jackpot_epoch_id
)
```

> The jackpot seed is **deterministic and auditable, but it is not consensus randomness and
> does not affect block validity.** Every input is already finalized on-chain; the seed is
> consumed only by the off-chain/indexer promotion logic. Anyone can recompute and verify
> the winner from public chain data.

### 5.2 Anti-whale weighting

Avoid pure proportional weighting (whales dominate) and avoid fully inverted weighting
(contributing less gives better odds). Use a capped/flattened curve:

```text
tickets_i = 1 + floor(sqrt(contribution_units_i))
```

Optionally cap `tickets_i <= TICKET_CAP` to further flatten the top. Winner is the
participant whose cumulative ticket interval contains `jackpot_seed mod total_tickets`.

Example: a participant with 100 contribution units gets `1 + floor(sqrt(100)) = 11`
tickets; one with 10,000 units gets `1 + 100 = 101` tickets — 100× the contribution yields
~9× the tickets, not 100×.

## 6. Sybil resistance

Square-root weighting blunts but does not eliminate Sybil splitting: an attacker could split
a large stake into many small identities, each getting a base ticket of 1. Mitigations
(layer 4/5):

- **Wallet-level participation receipts / registration (Binary Identity).** Opt-in: a
  participant registers a Binary Identity (doc 06) and accrues receipts for useful work /
  stake-time / node uptime. Unregistered fragments are not counted; this raises the cost of
  mass identity creation.
- **Minimum useful-contribution threshold** per identity to earn even the base ticket.
- **Stake-time / work weighting** rather than raw address count, so splitting does not
  multiply effective contribution.
- **Exclusion of aggregation addresses** (below).
- **Published methodology + recomputable result**, so manipulation is detectable.

Residual risk: a determined attacker who performs *genuine* distributed work/stake across
many real identities gains some edge; the sqrt curve and registration friction bound it.
This is documented, not claimed impossible (see doc 08).

## 7. Exclusions (app-layer promotional policy, NOT consensus)

> Pool / exchange / custodial / operator-aggregation address exclusion is **app-layer
> promotional policy in v1. It is not a consensus validity rule.** Address classification
> is subjective and addresses rotate, so a blacklist is governance, not protocol.

- Excluded from the **individual** major reward: known pool mining payout addresses,
  exchange deposit/withdrawal clusters, custodial wallets, and operator aggregation
  addresses.
- The exclusion registry is published by the foundation, versioned, and auditable.
- Misclassification is handled by an appeals/correction process at the promotion layer, not
  by forking the chain.

## 8. Behavior under degradation

If the chain is degraded (doc 02) or no Joint Anchor activates for the epoch:

- **No winner is drawn.**
- The **pool rolls forward** to the next epoch.
- Jackpot eligibility is frozen at the `BONUS_ESCROWED` rung and resumes on `RECOVERED`.

This ties the incentive directly to Binary Chain *health*: the event only fires when the
two chains successfully couple.

## 9. Compliance (read before any public marketing)

**This mechanism can trigger gambling / lottery / sweepstakes regulation if marketed
incorrectly. This document does not provide legal advice; obtain jurisdiction-specific
counsel before launch.**

- **Do not** use the word "lottery" (or "raffle"/"gambling") in protocol docs, wallet UI,
  or marketing. Use the approved terms in §1.
- **Prefer app-layer / foundation-run promotion first**, with on-chain participation proofs,
  rather than a protocol-native draw.
- If operated as a consumer promotion, include: **official rules**, **jurisdiction
  controls** (geofencing/exclusions where required), **tax reporting** (e.g. winner
  reporting thresholds), and a **no-purchase-necessary / alternative free entry** pathway.
- Eligibility tied to *useful contribution* (work/stake/uptime) rather than *purchase*
  helps, but does not by itself resolve regulatory classification.
- Keep the **first version off-chain / app-layer** using on-chain participation proofs;
  this reduces both protocol risk and legal exposure and lets rules adapt per jurisdiction.

## 10. Auditability summary

Although operated off-chain, every input is on-chain and the procedure is deterministic:

1. Participation proofs — derived from accepted chain data (work, stake-time, receipts).
2. Eligibility + exclusions — published, versioned registry.
3. Seed — recomputable hash of finalized chain values.
4. Weighting — published formula (`1 + floor(sqrt(units))`).
5. Winner — recomputable by anyone from the above.

The foundation publishes the inputs and the result; third parties can verify independently.
