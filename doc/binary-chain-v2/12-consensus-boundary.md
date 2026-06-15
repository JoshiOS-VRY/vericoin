# Binary Chain v2 — Consensus Boundary Analysis (Gatekeeper Document)

> This is the authoritative classification document for Binary Chain v2.
> It is written **first** and **gates every other document** in this set.
> If any other doc proposes putting a feature in a layer that contradicts this
> table, this document wins and the other doc must be corrected.

## Purpose

Binary Chain v2 combines a real consensus engine (DACE) with two adoption layers
(Binary Jackpot Rewards, Binary Address). The single biggest risk to the project
is **accidentally implementing promotional or subjective logic inside consensus**,
where it can never be changed without a hard fork, becomes a legal liability, or
turns into a consensus footgun.

Every feature below is classified into exactly one primary layer. The rule is:
**when in doubt, push the feature outward** (toward app/indexer/wallet), never
inward (toward consensus).

## The six layers

| # | Layer | Definition | Change cost |
|---|-------|------------|-------------|
| 1 | **Consensus-critical** | Affects block/transaction validity; must be byte-identical across all honest nodes; replayable from chain data alone | Hard fork |
| 2 | **Node policy / RPC** | Local node behavior, metrics, warnings, relay policy; never changes validity | Software update, no fork |
| 3 | **Wallet / library** | Key management, address encoding, signing, descriptors; client-side only | Wallet release |
| 4 | **Explorer / indexer** | Derived views computed from accepted chain data; read-only | Indexer deploy |
| 5 | **App-layer / foundation promotion** | Off-chain promotional mechanics, eligibility, payouts initiated by a human/foundation process | Policy/config change |
| 6 | **Documentation only** | Naming, framing, rationale; no code | Doc edit |

## Authoritative classification

| Feature | Layer | Notes / boundary rationale |
|---------|-------|----------------------------|
| DACE Joint Anchor object + lifecycle (`Observed → Certified → Activated`) | **1 Consensus** | Already implemented in `vericoin/src/dace/`. The coupling spine. |
| Extended 180-byte block header (`pairedAnchorRef`, `beaconRef`, `rewardAccumulatorRoot`, `epochIndex`) | **1 Consensus** | Already defined in `primitives/block.h`. The canonical commitment surface. No second TLV layer. |
| Deterministic beacon selection (height-based, `K_BEACON`-deep) | **1 Consensus** | DACE-2. Must be replayable from headers. |
| Bonded ticket registry + committee sortition + slashing | **1 Consensus** | DACE-3. Validity of certified anchors depends on it. |
| `PairedHeaderStore` ingestion + paired-header SPV validation | **1 Consensus / node** | DACE-7. Storage is node-local, but the *validation rules* that gate activation are consensus. |
| σ / φ cross-chain reward accumulators + accumulator roots | **1 Consensus** | DACE-5, already part of DACE. Roots are committed in headers. |
| Pull-based reward claim redemption (Merkle proof against activated JA) | **1 Consensus** | DACE-5. Claim tx validity is consensus; *who chooses to claim* is wallet. |
| Degradation ladder — the part that **modulates σ/φ accumulator roots** | **1 Consensus** | Only if derived strictly from canonical anchor/epoch state (see footgun rule below). |
| Expired-claim destination (`ClaimExpiryEpochs` → burn/treasury) | **1 Consensus (pre-existing)** | v2 **reads** the existing destination; it does **not** add a new one to fund the jackpot. |
| `consecutive_missed_anchors` **as a warning/metric** | **2 Node policy / RPC** | Surfaced via `binarychain_status` / `binarychain_degradation`. |
| Stale-coupling freshness signals (peer lag, header availability, gossip timing) | **2 Node policy / RPC** | Local health only. **Must never feed reward-root computation.** |
| Degradation **WARNING** surface | **2 Node policy / RPC** | Informational. |
| New RPCs (`binarychain_degradation`, `binarychain_reward_accumulators`, `binarychain_participation`) | **2 Node policy / RPC** | Expose raw, canonical data. Not winner selection. |
| Binary Address (`vbc1…`) encode/decode | **3 Wallet / library** | Dual-chain descriptor wrapping two native addresses. Consensus never sees it. |
| `getbinaryaddress` / `decodebinaryaddress` | **3 Wallet / library** | Wallet RPCs at the `key_io` layer. |
| Binary Identity opt-in participation receipts | **3 Wallet / 5 App** | Wallet generates; foundation/app consumes for eligibility. |
| Jackpot pool size (coin-denominated) display | **4 Explorer / indexer** | Computed from on-chain treasury/sponsor/rollover state. |
| Jackpot **USD estimated value** | **4 Explorer / indexer** | Display only. **No USD oracle in consensus.** |
| Jackpot eligibility computation (contribution scoring, ticket weighting) | **4 Explorer / indexer** | Reads canonical chain data; produces a candidate set. Not in `dace/`. |
| Jackpot **winner selection v1** | **5 App / foundation** | Deterministic + auditable, but **not consensus randomness**; no effect on block validity. |
| Pool / exchange / custodial **exclusion registry** | **5 App / foundation promotion** | Subjective, addresses rotate → governance, **never a consensus validity rule**. |
| Sponsor / treasury jackpot funding deposits | **5 App / foundation** | Ordinary on-chain transactions to a promotion-controlled address. |
| Jackpot constants (epoch length, split %, funding bps) | **5 App config / docs** | Live in app/indexer config + docs, **never `consensus/params.h`** in v2. |
| Compliance rules (no-purchase-necessary, jurisdiction, official rules) | **5 App / 6 Docs** | Promotional governance. |
| Naming hierarchy (Binary Chain / v2 / DACE / BiCED) | **6 Documentation** | See doc 00. |

## Consensus footgun guard (the one rule everyone forgets)

`consecutive_missed_anchors` is **consensus-usable only if it is computed strictly
from canonical epoch/anchor state visible in the accepted chain.**

- Local peer lag, local header availability, local gossip timing, and local
  dashboard health **must never directly change σ/φ accumulator roots**.
- Any node-local freshness signal is for **warnings and metrics only**.
- If two honest nodes with different peer connectivity could compute a different
  degradation state, then that degradation state **must not** influence reward
  roots — it can only influence layer-2 warnings.

A practical consequence: the degradation state that modulates economics is derived
from "how many consecutive coupling epochs have failed to produce an *activated*
Joint Anchor in the accepted chain," which is itself a canonical, replayable fact.

## Jackpot leakage guards (close the side doors)

1. **No new consensus money destination for the jackpot.** Expired claims already
   revert to burn/treasury via `ClaimExpiryEpochs`. v2 funds the jackpot from that
   *existing* destination at the app/promotion layer. Do not add a consensus rule
   that sends expired rewards to a "jackpot pool" address.
2. **No consensus winner selection.** The jackpot seed is built from values already
   finalized on-chain (beacon window root, VRC certificate root, previous seed,
   epoch id), but it is consumed off-chain. It is **deterministic and auditable,
   not consensus randomness, and does not affect block validity.**
3. **No subjective classification in consensus.** Exclusion of pools/exchanges/
   custodians is promotional policy. A blacklist is governance.
4. **No USD in consensus.** Coin amounts are consensus/display; USD is display only.

## Promotion to consensus (Phase 4 only)

A feature may move *inward* a layer (e.g. app → consensus) only when **all** of:

1. It has run safely at the outer layer through at least one full mainnet Jackpot
   Epoch and a security review.
2. It can be made fully deterministic and replayable from chain data.
3. It introduces no subjective input (no address classification, no USD, no
   off-chain oracle).
4. It passes the same Phase 2/3 gating used for DACE (see doc 09).

Until then, the default answer to "should this be in consensus?" is **no**.
