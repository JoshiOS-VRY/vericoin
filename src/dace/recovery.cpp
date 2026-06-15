// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/recovery.h>

#include <amount.h>
#include <consensus/params.h>
#include <pubkey.h>

#include <set>

namespace dace {

bool ValidateRecoveryAnchor(const RecoveryAnchor& anchor,
                            const TicketRegistry& registry,
                            const StaleCouplingTracker& tracker,
                            const Consensus::Params& params,
                            std::string& err)
{
    if (!anchor.IsRecovery()) {
        err = "recovery: anchor missing RECOVERY_FLAG_BIT";
        return false;
    }
    if (tracker.WarningLevel(params) != StaleCouplingTracker::Level::RecoveryEligible) {
        err = "recovery: chain not yet recovery-eligible (S_MAX not reached)";
        return false;
    }
    if (!anchor.beacon_proof.Verify(params, err)) {
        return false;
    }

    // Quorum check uses total bonded stake at the stalled epoch.
    const uint32_t base_epoch = anchor.BaseEpoch();
    const std::vector<Ticket> all_bonded = registry.ActiveAtEpoch(base_epoch);

    // Compute total bonded stake amount across the registry.
    CAmount total_bonded = 0;
    for (const Ticket& t : all_bonded) total_bonded += t.stake_amount;

    // Sum bonded stake of signers, verifying each signature against operator pubkey.
    const uint256 payload = anchor.GetPartialHash();
    std::set<uint16_t> seen;
    CAmount signing_bonded = 0;
    for (const CommitteeSignature& csig : anchor.signature.signatures) {
        if (csig.committee_index >= all_bonded.size()) {
            err = "recovery: signer index out of range";
            return false;
        }
        if (!seen.insert(csig.committee_index).second) {
            err = "recovery: duplicate signer";
            return false;
        }
        const Ticket& t = all_bonded[csig.committee_index];
        if (!t.operator_pubkey.IsFullyValid() ||
            !t.operator_pubkey.Verify(payload, csig.sig)) {
            err = "recovery: invalid signature";
            return false;
        }
        signing_bonded += t.stake_amount;
    }

    // recovery threshold: signing_bonded * den >= total_bonded * num
    if (signing_bonded * params.RecoveryThresholdDenominator
        < total_bonded * params.RecoveryThresholdNumerator) {
        err = "recovery: insufficient bonded stake signed";
        return false;
    }
    return true;
}

RecoveryAnchor BuildRecoveryAnchor(uint32_t stalled_epoch,
                                   const JointAnchor& last_active,
                                   const Beacon& fresh_beacon,
                                   const BeaconDepthProof& proof,
                                   const uint256& vrc_checkpoint_hash,
                                   uint32_t vrc_checkpoint_height)
{
    RecoveryAnchor a;
    a.SetRecovery(stalled_epoch);
    a.prev_anchor_hash = last_active.GetHash();
    a.beacon_ref = fresh_beacon.beacon_hash;
    a.beacon_proof = proof;
    a.vrc_checkpoint_hash = vrc_checkpoint_hash;
    a.vrc_checkpoint_height = vrc_checkpoint_height;
    a.reward_root_vrc_prev = last_active.reward_root_vrc_prev; // preserved
    a.reward_root_vrm_prev = last_active.reward_root_vrm_prev; // preserved
    a.committee_root = last_active.committee_root;             // preserved
    // Signature is filled by collecting CommitteeSignature entries from
    // bonded operators above the recovery threshold.
    return a;
}

} // namespace dace
