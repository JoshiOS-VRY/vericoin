// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_STALE_COUPLING_H
#define BITCOIN_DACE_STALE_COUPLING_H

#include <consensus/params.h>
#include <stdint.h>
#include <string>

namespace dace {

/** Stale-coupling tracker. See DACE-7.
 *
 *  When paired-chain data needed by DACE validation (paired headers, an
 *  expected JA, a beacon depth proof) is unavailable, the local chain enters
 *  stale-coupling mode: local block processing continues, finality upgrades
 *  pause, and the binarychain_status RPC surfaces a warning.
 *
 *  This module tracks how long the current stall has lasted (in coupling
 *  epochs) and exposes:
 *    - StallEpochs(): how many consecutive epochs the local chain has been
 *      stale-coupled
 *    - WarningLevel(): None | Warn (above StaleGraceEpochs) | RecoveryEligible
 *      (at or above StaleMaxEpochs)
 */
class StaleCouplingTracker {
public:
    enum class Level {
        None,
        Warn,
        RecoveryEligible,
    };

    /** Called at each VRC block at coupling-epoch boundaries. */
    void OnEpochBoundary(uint32_t current_epoch, bool advanced);

    /** Mark the chain as stale-coupled (paired data not available). */
    void EnterStale(const std::string& reason);

    /** Mark the chain as caught up (paired data available, finality
     *  upgrades resumed). Resets the counter. */
    void LeaveStale();

    uint32_t StallEpochs() const { return stall_epochs_; }
    Level WarningLevel(const Consensus::Params& params) const;
    const std::string& Reason() const { return reason_; }

private:
    uint32_t stall_epochs_{0};
    std::string reason_;
};

} // namespace dace

#endif // BITCOIN_DACE_STALE_COUPLING_H
