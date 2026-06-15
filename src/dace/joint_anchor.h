// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_JOINT_ANCHOR_H
#define BITCOIN_DACE_JOINT_ANCHOR_H

#include <dace/beacon.h>
#include <dace/ticket_registry.h>
#include <serialize.h>
#include <uint256.h>

#include <vector>

namespace dace {

/** One committee member's signature inside a JointAnchor's aggregated
 *  signature payload. See DACE-4 §"Signature scheme".
 *
 *  Phase 1 (initial activation): list of (committee_index, ECDSA sig) pairs.
 *  Phase 4 (future): Schnorr aggregate.
 */
struct CommitteeSignature {
    uint16_t committee_index{0};   ///< index into committee_root (ticket_id sorted)
    std::vector<unsigned char> sig;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(committee_index);
        READWRITE(sig);
    }
};

/** Aggregated signature object inside a JointAnchor. */
struct QuorumSignature {
    std::vector<CommitteeSignature> signatures;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(signatures);
    }
};

/** Joint Anchor object. See DACE-4.
 *
 *  Three-phase lifecycle:
 *    1. observed: all non-signature fields known locally
 *    2. certified: signature contains >= q-quorum sigs over partial_hash()
 *    3. activated: handled by AnchorLifecycle (see anchor_lifecycle.h)
 */
struct JointAnchor {
    uint32_t        epoch_index{0};
    uint256         prev_anchor_hash;          ///< JA_{e-1} or 0 for genesis
    uint256         beacon_ref;                ///< beacon_e.hash per DACE-2
    BeaconDepthProof beacon_proof;             ///< K-deep succeeding VRM headers
    uint256         vrc_checkpoint_hash;       ///< VRC tip at epoch end
    uint32_t        vrc_checkpoint_height{0};
    uint256         reward_root_vrc_prev;      ///< rewardAccumulatorRoot for VRC at epoch-1
    uint256         reward_root_vrm_prev;      ///< rewardAccumulatorRoot for VRM at epoch-1
    uint256         committee_root;            ///< per DACE-3 Merkle root of selected ticket_ids
    QuorumSignature signature;                 ///< populated in certified phase

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(epoch_index);
        READWRITE(prev_anchor_hash);
        READWRITE(beacon_ref);
        READWRITE(beacon_proof);
        READWRITE(vrc_checkpoint_hash);
        READWRITE(vrc_checkpoint_height);
        READWRITE(reward_root_vrc_prev);
        READWRITE(reward_root_vrm_prev);
        READWRITE(committee_root);
        // Signature is serialized only after certification. For wire/disk
        // representation we always include it (may be empty when observed).
        READWRITE(signature);
    }

    /** Full anchor hash, including signature. Used as pairedAnchorRef. */
    uint256 GetHash() const;

    /** Hash of non-signature fields, used as the signing payload. */
    uint256 GetPartialHash() const;

    bool IsObserved() const;
    bool IsCertifiedShape() const; // signature has at least one sig
};

/** Verify the aggregated signature against the committee.
 *  - Looks up each committee_index in `committee` (ticket_id-sorted).
 *  - Verifies each sig against the corresponding operator_pubkey over
 *    GetPartialHash().
 *  - Checks the quorum threshold q = num/den from params.
 *
 *  Returns true if the anchor is *certified* (quorum reached and all sigs
 *  valid). On false, `err` describes the failure mode.
 */
bool VerifyCertification(const JointAnchor& anchor,
                         const std::vector<Ticket>& committee,
                         const Consensus::Params& params,
                         std::string& err);

} // namespace dace

#endif // BITCOIN_DACE_JOINT_ANCHOR_H
