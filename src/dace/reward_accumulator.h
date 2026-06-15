// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_REWARD_ACCUMULATOR_H
#define BITCOIN_DACE_REWARD_ACCUMULATOR_H

#include <amount.h>
#include <consensus/params.h>
#include <script/script.h>
#include <serialize.h>
#include <uint256.h>

#include <map>
#include <vector>

namespace dace {

/** Which chain originated this claim. */
enum class ClaimSourceChain : uint8_t {
    VRM = 1,   ///< VRM funded the pool (paying VRC committee operators)
    VRC = 2,   ///< VRC funded the pool (paying VRM beacon miners)
};

/** Whose claim it is, by leaf semantics. */
enum class ClaimRecipientKind : uint8_t {
    VRC_TICKET_OPERATOR = 1,   ///< paid by a VRM block's diverted subsidy
    VRM_BEACON_MINER    = 2,   ///< paid by a VRC block's diverted fees
};

/** One claim leaf accumulated by a single block. */
struct ClaimLeaf {
    ClaimSourceChain   source_chain{ClaimSourceChain::VRM};
    uint256            source_block;       ///< block hash that emitted this leaf
    uint32_t           epoch{0};
    ClaimRecipientKind recipient_kind{ClaimRecipientKind::VRC_TICKET_OPERATOR};
    uint160            recipient_pkh;      ///< hash160 of recipient pubkey
    CAmount            amount{0};

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        uint8_t sc = static_cast<uint8_t>(source_chain);
        uint8_t rk = static_cast<uint8_t>(recipient_kind);
        READWRITE(sc);
        READWRITE(source_block);
        READWRITE(epoch);
        READWRITE(rk);
        READWRITE(recipient_pkh);
        READWRITE(amount);
        if (ser_action.ForRead()) {
            source_chain = static_cast<ClaimSourceChain>(sc);
            recipient_kind = static_cast<ClaimRecipientKind>(rk);
        }
    }

    /** Leaf hash for the per-epoch Merkle accumulator. */
    uint256 GetLeafHash() const;
};

/** Per-chain reward accumulator state. One instance lives per active epoch
 *  per local chain; closed at epoch end into a finalized root. */
class RewardAccumulator {
public:
    explicit RewardAccumulator(uint32_t epoch) : epoch_(epoch) {}

    /** Append a leaf created by the local block at `block_height`.
     *  Caller is responsible for computing the divert amount per DACE-5. */
    void AppendLeaf(const ClaimLeaf& leaf);

    /** Number of leaves accumulated so far. */
    size_t LeafCount() const { return leaves_.size(); }

    /** Current running Merkle root. Recomputed each call. */
    uint256 CurrentRoot() const;

    /** Build the Merkle proof for a given leaf index. */
    bool BuildProof(size_t leaf_index, std::vector<uint256>& out_path) const;

    /** Access the underlying leaves (e.g., for snapshotting). */
    const std::vector<ClaimLeaf>& Leaves() const { return leaves_; }

    uint32_t Epoch() const { return epoch_; }

private:
    uint32_t epoch_;
    std::vector<ClaimLeaf> leaves_;
};

/** Compute the divert amount per DACE-5 for a VRM block. */
CAmount DivertVrmSubsidy(CAmount base_subsidy, const Consensus::Params& params);

/** Compute the divert amount per DACE-5 for a VRC block. */
CAmount DivertVrcFees(CAmount block_fees, const Consensus::Params& params);

/** The consensus-defined unspendable-except-by-claim escrow scriptPubKey on
 *  each chain. Returns a script that only valid claim-redemption transactions
 *  may consume. */
CScript GetEscrowScript(ClaimSourceChain chain);

} // namespace dace

#endif // BITCOIN_DACE_REWARD_ACCUMULATOR_H
