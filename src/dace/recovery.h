// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_RECOVERY_H
#define BITCOIN_DACE_RECOVERY_H

#include <dace/joint_anchor.h>
#include <dace/stale_coupling.h>
#include <dace/ticket_registry.h>
#include <consensus/params.h>

#include <string>
#include <vector>

namespace dace {

/** A recovery anchor object. Same wire shape as JointAnchor with a flag bit
 *  set in epoch_index's high byte to distinguish it. Quorum threshold is
 *  RecoveryThresholdNumerator/Denominator of total bonded stake, not the
 *  selected committee.
 *
 *  Recovery is rare-path. It exists so a stalled coupling can resume without
 *  requiring an off-chain coordination upgrade. See DACE-4 §"Liveness and
 *  recovery".
 */
struct RecoveryAnchor : public JointAnchor {
    static constexpr uint32_t RECOVERY_FLAG_BIT = 0x80000000u;

    bool IsRecovery() const { return (epoch_index & RECOVERY_FLAG_BIT) != 0; }
    uint32_t BaseEpoch() const { return epoch_index & ~RECOVERY_FLAG_BIT; }
    void SetRecovery(uint32_t base_epoch) {
        epoch_index = base_epoch | RECOVERY_FLAG_BIT;
    }
};

/** Validate that a recovery anchor is admissible:
 *    1. Stale-coupling has reached RecoveryEligible.
 *    2. The recovery signature collection covers >= RecoveryThreshold of
 *       total bonded stake (not just selected committee).
 *    3. The beacon depth proof is still valid.
 *
 *  Returns true on success; on false `err` describes the reason.
 */
bool ValidateRecoveryAnchor(const RecoveryAnchor& anchor,
                            const TicketRegistry& registry,
                            const StaleCouplingTracker& tracker,
                            const Consensus::Params& params,
                            std::string& err);

/** Build a recovery anchor candidate, given a committee of bonded operators
 *  meeting the recovery threshold. Operators sign anchor.GetPartialHash()
 *  individually; the resulting signatures populate anchor.signature. */
RecoveryAnchor BuildRecoveryAnchor(uint32_t stalled_epoch,
                                   const JointAnchor& last_active,
                                   const Beacon& fresh_beacon,
                                   const BeaconDepthProof& proof,
                                   const uint256& vrc_checkpoint_hash,
                                   uint32_t vrc_checkpoint_height);

} // namespace dace

#endif // BITCOIN_DACE_RECOVERY_H
