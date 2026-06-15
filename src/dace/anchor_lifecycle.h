// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_ANCHOR_LIFECYCLE_H
#define BITCOIN_DACE_ANCHOR_LIFECYCLE_H

#include <dace/joint_anchor.h>
#include <dace/ticket_registry.h>
#include <consensus/params.h>

#include <map>
#include <memory>

namespace dace {

enum class AnchorPhase {
    None = 0,
    Observed,
    Certified,
    Activated,
};

/** Per-node state machine for the JA lifecycle.
 *
 *  Owned at chain-state granularity. Mutations are atomic w.r.t. block
 *  connect/disconnect (rewinds supported via Rewind()).
 *
 *  Activation rule (DACE-4 §"Activation rule (concrete)"):
 *    A certified JA_e becomes activated at VRC height h_c when:
 *      ja.certified_at_vrc_height + BeaconEpochVRC <= h_c
 *      AND
 *      ja.hash has been included in at least one VRM coinbase commitment whose
 *      containing VRM block has BEACON_EPOCH_VRM_CONF confirmations (default 6).
 *
 *  This lifecycle object tracks observation/certification at the node level
 *  but the *activation event* itself is published to the validation layer
 *  via PromoteIfActivated() which is called from ConnectBlock.
 */
class AnchorLifecycle {
public:
    AnchorLifecycle() = default;

    /** Returns the currently-activated anchor, or nullptr if none.
     *  The latest activated anchor is what blocks must reference via
     *  CBlockHeader::pairedAnchorRef. */
    const JointAnchor* Active() const;

    /** Observe an anchor candidate (any phase). Stores it indexed by
     *  partial-hash. Idempotent. */
    void Observe(const JointAnchor& anchor);

    /** Promote an observed anchor to certified, given a verified quorum
     *  signature. Returns false if the partial-hash is unknown.
     *
     *  Should be called from net_processing when a `ja` message arrives with
     *  a quorum signature, or when local `jasig` aggregation reaches q. */
    bool Certify(const uint256& partial_hash,
                 const QuorumSignature& sig,
                 const std::vector<Ticket>& committee,
                 const Consensus::Params& params,
                 std::string& err);

    /** Try to promote a certified anchor to activated, given the current
     *  VRC tip height and the set of VRM coinbase JA inclusions observed
     *  so far. Called from ConnectBlock.
     *
     *  Returns the activated anchor on promotion, nullptr otherwise. */
    const JointAnchor* PromoteIfActivated(int current_vrc_height,
                                          const std::map<uint256, int>& vrm_inclusions,
                                          const Consensus::Params& params);

    /** Rewind state to a target VRC height (e.g., due to chain reorg). */
    void Rewind(int target_vrc_height, const Consensus::Params& params);

    /** Detect equivocation: returns evidence if `anchor` would conflict with
     *  another observed anchor for the same epoch. */
    bool DetectEquivocation(const JointAnchor& anchor, TicketSlashEvidence& evidence) const;

private:
    struct Entry {
        JointAnchor anchor;
        AnchorPhase phase{AnchorPhase::Observed};
        int certified_at_vrc_height{-1};
        bool included_in_vrm{false};
    };

    std::map<uint256, Entry> entries_;   ///< keyed by partial_hash
    uint256 active_partial_hash_;        ///< partial_hash of Active()
};

} // namespace dace

#endif // BITCOIN_DACE_ANCHOR_LIFECYCLE_H
