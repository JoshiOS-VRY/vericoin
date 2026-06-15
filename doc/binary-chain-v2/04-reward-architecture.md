# Binary Chain v2 — Reward Architecture

> Consensus layer (layer 1) for accumulators and claims. Implements DACE-5 and adds the
> base-vs-bonus framing. The jackpot funding skim referenced here is **app-layer**
> (doc 05); nothing in this doc adds a new consensus money destination.

## 1. The sacred/programmable split

```text
BASE (sacred, never touched by Binary Chain coupling/degradation/jackpot):
  VRM miners earn normal VRM PoWT block subsidy + fees.
  VRC stakers earn normal VRC PoST staking rewards + fees.

BONUS (programmable, the only thing degradation/jackpot touch):
  sigma: VRM diverts DivertSigmaBpsVRM (4%) of *base subsidy* into a VRC-bound reward pool.
  phi:   VRC diverts DivertPhiBpsVRC (10%) of *block fees* into a VRM-bound reward pool.
```

Important framing: σ/φ are **diversions out of amounts that would otherwise be base**.
Reducing them during degradation therefore *returns* value to base miners/stakers — base
is strictly protected. The bonus is the programmable surface.

## 2. Delayed reward accumulators (no live foreign payees)

The core anti-circularity rule (from the DACE research line): **a VRM block never pays a
live VRC payee, and a VRC block never pays a live VRM payee.** Instead each chain escrows
locally and publishes a Merkle root; claims are redeemed later on the destination chain.

### 2.1 Funding (each block ≥ activation)

- VRM block: divert `σ * base_subsidy` into the local escrow `scriptPubKey`
  (`dace::GetEscrowScript`), append a claim leaf for VRC ticket operators.
- VRC block: divert `φ * block_fees` into the local escrow, append a claim leaf for VRM
  beacon miners.

Escrow outputs are unspendable except via a valid claim redemption.

### 2.2 Claim leaves (`dace/reward_accumulator.h`)

```cpp
enum class ClaimSourceChain  : uint8_t { VRM = 1, VRC = 2 };
enum class ClaimRecipientKind: uint8_t { VRC_TICKET_OPERATOR = 1, VRM_BEACON_MINER = 2 };

struct ClaimLeaf {
    ClaimSourceChain   source_chain;
    uint256            source_block;
    uint32_t           epoch;
    ClaimRecipientKind recipient_kind;
    uint160            recipient_pkh;   // P2PKH of the destination-chain recipient
    CAmount            amount;
};
```

Leaves accumulate into a running Merkle root; at epoch close the root becomes
`rewardAccumulatorRoot_e`, committed in (a) the `rewardAccumulatorRoot` header field of
every block in epoch `e+1`, and (b) the `reward_root_vrc_prev` / `reward_root_vrm_prev`
fields of `JA_{e+1}`.

### 2.3 Degradation modulation

The σ/φ accrual multiplier from the ladder (doc 02) scales the diverted `amount` before it
is escrowed and before the leaf is appended. Because the multiplier is a pure function of
canonical `consecutive_missed_anchors`, every node computes the same leaves and the same
root. This is the **only** consensus coupling between degradation and rewards.

## 3. Pull-based claims

### 3.1 Redemption (on the destination chain)

```cpp
struct ClaimRedemptionWitness {
    uint32_t           epoch;
    uint256            ja_full_hash;     // an ACTIVATED JA that anchored the reward root
    ClaimLeaf          leaf;
    std::vector<uint256> merkle_path;
};
```

```text
ValidateClaim(witness, tx):
  1. ja_full_hash is the hash of an ACTIVATED Joint Anchor.
  2. The JA's reward_root_{vrc|vrm}_prev is reconstructable from leaf + merkle_path.
  3. tx output script == P2PKH(leaf.recipient_pkh) for leaf.amount.
  4. (epoch, recipient_pkh, source) not already in the ClaimSpentSet (single-use).
  5. The local escrow holds >= leaf.amount.
```

Claim marker `CLAIM_MARKER = "DCAC"`. The redeeming transaction is a normal
destination-chain transaction; **no block-validity rule depends on live foreign payee
data** — only on an already-activated, already-committed Merkle root.

### 3.2 Automatic vs claim-based

**Claim-based (pull).** Rationale: the recipient's destination-chain address is not known
to the source chain at funding time, and forcing a push would re-introduce live foreign
payees. Wallets can auto-redeem on the user's behalf (scan `rewardAccumulatorRoot`s,
build witnesses), so the UX feels automatic while consensus stays pull-based.

### 3.3 Double-claim prevention

The `ClaimSpentSet` records `(epoch, recipient_pkh, source_chain)` (or leaf hash) as spent
on first redemption; a second attempt fails validation (rule 4). This set is part of
consensus state on the destination chain.

## 4. Expiry and recycling

- Unredeemed claims expire after `ClaimExpiryEpochs` (1024 mainnet / 256 binarytest).
- On expiry the escrowed funds revert to the **existing** consensus-defined destination
  (burn or treasury fund, frozen at Phase 0). **v2 does not add a new destination.**
- **Jackpot funding from expired rewards is app-layer:** the promotion service may, as a
  matter of foundation policy, fund the jackpot pool from the treasury that already
  receives expired escrow. It does **not** insert a consensus rule routing expired escrow
  to a "jackpot pool" address (doc 12 leakage guard #1).

## 5. How this differs from a bridge

| Property | Binary Chain reward | Token bridge |
|----------|---------------------|--------------|
| Asset moved | None — payouts are **native** VRM/VRC minted/escrowed on their own chain | Wrapped/locked asset moves across |
| Custody | No custodian; escrow is consensus scriptPubKey | Custodian or multisig/relayer set |
| Trust | Delayed, activated, Merkle-proven on the paying chain | Relayers/validators attest foreign state live |
| Failure mode | Degrades bonus; base unaffected | Funds frozen/stolen if bridge compromised |
| Foreign payee | Never required at funding time | Required (mint to foreign address) |

There are **no wrapped assets** and **no bridge**. A VRC contributor who earns a
VRM-denominated Binary Chain bonus receives **native VRM** on the Verium chain by
redeeming a claim there; the VRM was escrowed from real VRM subsidy on the Verium chain.

## 6. Reward flow summary

```text
VRM miners earn normal VRM rewards.            (base, sacred)
VRC stakers earn normal VRC rewards.           (base, sacred)

Each block escrows sigma/phi into a local pool and appends a claim leaf. (bonus)
At epoch close, the accumulator root is committed in headers and the next JA.

When a Joint Anchor activates:
  eligible VRM contributors may claim VRC-denominated Binary Chain rewards
    (redeemed natively on VeriCoin);
  eligible VRC contributors may claim VRM-denominated Binary Chain rewards
    (redeemed natively on Verium).

Degradation scales the bonus accrual multiplier (never base).
Expired claims revert to the existing burn/treasury destination.
```

## 7. Implementation status / gaps (see doc 10)

Implemented: `ClaimLeaf`, `ClaimRedemptionWitness`, `ClaimSpentSet`, `ValidateClaim`,
`GetEscrowScript`, `DivertVrmSubsidy`/`DivertVrcFees`, accumulator append on connect.

Gaps: claim validation is **not yet enforced** in validation/mempool; connect appends
leaves with empty `recipient_pkh` (filled by wallet policy later);
`binarychain_redeem_claim` only reserves the spent set (no witness assembly/broadcast yet);
the σ/φ degradation multiplier (doc 02) is not yet applied to accrual.
