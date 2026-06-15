// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/anchor_lifecycle.h>

namespace dace {

const JointAnchor* AnchorLifecycle::Active() const
{
    if (active_partial_hash_.IsNull()) return nullptr;
    auto it = entries_.find(active_partial_hash_);
    if (it == entries_.end()) return nullptr;
    if (it->second.phase != AnchorPhase::Activated) return nullptr;
    return &it->second.anchor;
}

void AnchorLifecycle::Observe(const JointAnchor& anchor)
{
    const uint256 key = anchor.GetPartialHash();
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        // Existing entry — keep its phase but refresh anchor body in case
        // signature shape changed.
        it->second.anchor = anchor;
        return;
    }
    Entry e;
    e.anchor = anchor;
    e.phase = AnchorPhase::Observed;
    entries_.emplace(key, std::move(e));
}

bool AnchorLifecycle::Certify(const uint256& partial_hash,
                              const QuorumSignature& sig,
                              const std::vector<Ticket>& committee,
                              const Consensus::Params& params,
                              std::string& err)
{
    auto it = entries_.find(partial_hash);
    if (it == entries_.end()) {
        err = "certify: unknown partial_hash";
        return false;
    }
    Entry& e = it->second;
    // Build a temp anchor with the candidate signature and verify against
    // committee.
    JointAnchor candidate = e.anchor;
    candidate.signature = sig;
    if (!VerifyCertification(candidate, committee, params, err)) {
        return false;
    }
    e.anchor.signature = sig;
    e.phase = AnchorPhase::Certified;
    return true;
}

const JointAnchor* AnchorLifecycle::PromoteIfActivated(
    int current_vrc_height,
    const std::map<uint256, int>& vrm_inclusions,
    const Consensus::Params& params)
{
    // Find the highest-epoch certified anchor that satisfies both rules:
    //   1. certified_at_vrc_height + BeaconEpochVRC <= current_vrc_height
    //   2. ja.hash (full hash) appears in vrm_inclusions with sufficient depth
    // and promote it to Activated. Higher-epoch anchors implicitly supersede
    // lower-epoch ones.
    const JointAnchor* promoted = nullptr;

    for (auto& kv : entries_) {
        Entry& e = kv.second;
        if (e.phase != AnchorPhase::Certified) continue;

        // Rule 1: epoch-delay check
        if (e.certified_at_vrc_height < 0) {
            e.certified_at_vrc_height = current_vrc_height; // first observation as certified
        }
        if (e.certified_at_vrc_height + params.BeaconEpochVRC > current_vrc_height) {
            continue;
        }

        // Rule 2: VRM coinbase inclusion with sufficient depth
        const uint256 ja_full_hash = e.anchor.GetHash();
        auto inc_it = vrm_inclusions.find(ja_full_hash);
        if (inc_it == vrm_inclusions.end()) continue;
        const int inclusion_depth = inc_it->second;
        // BEACON_EPOCH_VRM_CONF default 6 — kept as compile-time constant for
        // now; a future revision may make this a chainparam.
        constexpr int BEACON_EPOCH_VRM_CONF = 6;
        if (inclusion_depth < BEACON_EPOCH_VRM_CONF) continue;

        e.phase = AnchorPhase::Activated;
        e.included_in_vrm = true;
        // Track latest by epoch.
        if (promoted == nullptr || promoted->epoch_index < e.anchor.epoch_index) {
            promoted = &e.anchor;
            active_partial_hash_ = kv.first;
        }
    }
    return promoted;
}

void AnchorLifecycle::Rewind(int target_vrc_height, const Consensus::Params& params)
{
    // Demote anchors that were certified or activated only at heights above
    // the rewind target. Activation rule demotion may cascade to the active
    // anchor; in that case fall back to the highest-epoch still-activated
    // anchor with certified_at <= target_vrc_height.
    for (auto& kv : entries_) {
        Entry& e = kv.second;
        if (e.certified_at_vrc_height > target_vrc_height) {
            e.phase = AnchorPhase::Observed;
            e.certified_at_vrc_height = -1;
        } else if (e.phase == AnchorPhase::Activated &&
                   e.certified_at_vrc_height + params.BeaconEpochVRC > target_vrc_height) {
            e.phase = AnchorPhase::Certified;
        }
    }

    // Recompute active pointer.
    active_partial_hash_.SetNull();
    const JointAnchor* best = nullptr;
    uint256 best_key;
    for (const auto& kv : entries_) {
        if (kv.second.phase != AnchorPhase::Activated) continue;
        if (best == nullptr || best->epoch_index < kv.second.anchor.epoch_index) {
            best = &kv.second.anchor;
            best_key = kv.first;
        }
    }
    if (best != nullptr) active_partial_hash_ = best_key;
}

bool AnchorLifecycle::DetectEquivocation(const JointAnchor& anchor,
                                        TicketSlashEvidence& evidence) const
{
    for (const auto& kv : entries_) {
        const Entry& e = kv.second;
        if (e.anchor.epoch_index != anchor.epoch_index) continue;
        if (e.anchor.GetPartialHash() == anchor.GetPartialHash()) continue;
        // Same epoch, different partial_hash -> equivocation candidate.
        // For evidence, we need two signatures from the same operator over the
        // two conflicting partial_hashes. The caller (slashing pipeline) walks
        // overlapping committee members and constructs evidence per offender.
        evidence.epoch_index = anchor.epoch_index;
        evidence.ja_partial_hash_a = e.anchor.GetPartialHash();
        evidence.ja_partial_hash_b = anchor.GetPartialHash();
        // ticket_id and sigs filled in by the caller iterating committees.
        return true;
    }
    return false;
}

} // namespace dace
