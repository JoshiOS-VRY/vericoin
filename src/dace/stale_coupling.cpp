// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/stale_coupling.h>

#include <consensus/params.h>

namespace dace {

void StaleCouplingTracker::OnEpochBoundary(uint32_t /*current_epoch*/, bool advanced)
{
    if (advanced && reason_.empty()) {
        stall_epochs_ = 0;
    } else if (!reason_.empty()) {
        ++stall_epochs_;
    }
}

void StaleCouplingTracker::EnterStale(const std::string& reason)
{
    reason_ = reason;
}

void StaleCouplingTracker::LeaveStale()
{
    reason_.clear();
    stall_epochs_ = 0;
}

StaleCouplingTracker::Level StaleCouplingTracker::WarningLevel(
    const Consensus::Params& params) const
{
    if (stall_epochs_ >= static_cast<uint32_t>(params.StaleMaxEpochs)) {
        return Level::RecoveryEligible;
    }
    if (stall_epochs_ >= static_cast<uint32_t>(params.StaleGraceEpochs)) {
        return Level::Warn;
    }
    return Level::None;
}

} // namespace dace
