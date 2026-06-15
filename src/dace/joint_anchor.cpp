// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/joint_anchor.h>

#include <consensus/params.h>
#include <hash.h>
#include <pubkey.h>
#include <streams.h>

#include <set>

namespace dace {

uint256 JointAnchor::GetHash() const
{
    return SerializeHash(*this);
}

uint256 JointAnchor::GetPartialHash() const
{
    // Serialize the non-signature fields and hash them.
    CHashWriter w(SER_GETHASH, 0);
    w << epoch_index;
    w << prev_anchor_hash;
    w << beacon_ref;
    w << beacon_proof;
    w << vrc_checkpoint_hash;
    w << vrc_checkpoint_height;
    w << reward_root_vrc_prev;
    w << reward_root_vrm_prev;
    w << committee_root;
    return w.GetHash();
}

bool JointAnchor::IsObserved() const
{
    return !beacon_ref.IsNull()
        && !vrc_checkpoint_hash.IsNull()
        && !committee_root.IsNull();
}

bool JointAnchor::IsCertifiedShape() const
{
    return !signature.signatures.empty();
}

bool VerifyCertification(const JointAnchor& anchor,
                         const std::vector<Ticket>& committee,
                         const Consensus::Params& params,
                         std::string& err)
{
    if (!anchor.IsObserved()) {
        err = "anchor missing required fields";
        return false;
    }
    if (anchor.signature.signatures.empty()) {
        err = "anchor not signed";
        return false;
    }

    const uint256 payload = anchor.GetPartialHash();

    std::set<uint16_t> seen_indices;
    size_t valid_count = 0;
    for (const CommitteeSignature& csig : anchor.signature.signatures) {
        if (csig.committee_index >= committee.size()) {
            err = "committee_index out of range";
            return false;
        }
        if (!seen_indices.insert(csig.committee_index).second) {
            err = "duplicate committee_index in quorum signature";
            return false;
        }
        const CPubKey& pk = committee[csig.committee_index].operator_pubkey;
        if (!pk.IsFullyValid()) {
            err = "committee member has invalid pubkey";
            return false;
        }
        if (!pk.Verify(payload, csig.sig)) {
            err = "invalid sig from committee_index " + std::to_string(csig.committee_index);
            return false;
        }
        ++valid_count;
    }

    // Quorum check: valid_count * den >= committee.size() * num
    const int64_t num = params.CommitteeQuorumNumerator;
    const int64_t den = params.CommitteeQuorumDenominator;
    if (static_cast<int64_t>(valid_count) * den < static_cast<int64_t>(committee.size()) * num) {
        err = "quorum not reached";
        return false;
    }
    return true;
}

} // namespace dace
