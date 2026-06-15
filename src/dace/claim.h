// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_CLAIM_H
#define BITCOIN_DACE_CLAIM_H

#include <dace/joint_anchor.h>
#include <dace/reward_accumulator.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <consensus/params.h>
#include <script/script.h>

#include <set>

namespace dace {

/** Witness object for a claim redemption transaction. Serialized as part of
 *  the redemption tx's input scriptSig (or as separate witness data on
 *  segwit-capable chains). */
struct ClaimRedemptionWitness {
    uint32_t  epoch;
    uint256   ja_full_hash;            ///< activated JA that anchored reward_root
    ClaimLeaf leaf;
    std::vector<uint256> merkle_path;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(epoch);
        READWRITE(ja_full_hash);
        READWRITE(leaf);
        READWRITE(merkle_path);
    }
};

/** Tracker of already-redeemed leaves on the local chain. Persisted in
 *  LevelDB under "dace/claims-spent/". */
class ClaimSpentSet {
public:
    bool IsSpent(const uint256& leaf_hash) const {
        return spent_.find(leaf_hash) != spent_.end();
    }
    void MarkSpent(const uint256& leaf_hash) {
        spent_.insert(leaf_hash);
    }
    void UnmarkSpent(const uint256& leaf_hash) {
        spent_.erase(leaf_hash);
    }
    size_t Size() const { return spent_.size(); }

private:
    std::set<uint256> spent_;
};

/** Interface for the chain-local claim validator to look up activated JAs. */
class IActivatedAnchorView {
public:
    virtual ~IActivatedAnchorView() = default;
    virtual const JointAnchor* FindByHash(const uint256& ja_full_hash) const = 0;
    /** Returns the activated JA whose reward_root for the appropriate chain
     *  matches what the witness's epoch implies. */
    virtual const JointAnchor* FindForEpoch(uint32_t epoch) const = 0;
};

/** Validate a claim redemption transaction.
 *
 *  Checks:
 *    1. ja_full_hash references an activated Joint Anchor.
 *    2. The leaf reconstructs to the correct reward_root_*_prev from that JA.
 *    3. Leaf's recipient_pkh matches the tx's first output script.
 *    4. Leaf has not been previously redeemed in this chain.
 *    5. The escrow output has sufficient balance (tracked externally).
 *    6. The leaf is not past CLAIM_EXPIRY epochs.
 *
 *  Mutates `spent_set` on success (marks leaf hash as spent).
 */
bool ValidateClaim(const CTransaction& tx,
                   const ClaimRedemptionWitness& witness,
                   const IActivatedAnchorView& anchor_view,
                   ClaimSpentSet& spent_set,
                   uint32_t current_epoch,
                   ClaimSourceChain destination_chain,
                   const Consensus::Params& params,
                   std::string& err);

/** Encode a ClaimRedemptionWitness into a tx input scriptSig pattern. */
CScript BuildClaimScriptSig(const ClaimRedemptionWitness& witness);

/** Decode a tx input scriptSig into a ClaimRedemptionWitness. Returns false if
 *  the scriptSig is not a valid claim payload. */
bool ParseClaimScriptSig(const CScript& script_sig, ClaimRedemptionWitness& out);

} // namespace dace

#endif // BITCOIN_DACE_CLAIM_H
