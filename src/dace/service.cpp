// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/service.h>

#include <dace/activation.h>
#include <util/system.h>

#include <memory>

namespace dace {

namespace {
std::unique_ptr<Service> g_service;
} // namespace

void Service::Initialize(const Consensus::Params& /*params*/,
                         const std::string& /*datadir*/)
{
    if (!g_service) {
        g_service.reset(new Service());
        // Persistent state (ticket registry, claim-spent set, accumulators)
        // is loaded from LevelDB in a follow-up: Service::LoadFromDisk.
        // Phase 1 keeps state in-memory and rebuilds on restart.
    }
}

void Service::Teardown()
{
    g_service.reset();
}

Service& Service::Get()
{
    // Caller must have invoked Initialize.
    return *g_service;
}

bool Service::IsInitialized()
{
    return static_cast<bool>(g_service);
}

bool Service::GetSelectedBeacon(uint32_t epoch, Beacon& out) const
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = beacon_cache_.find(epoch);
    if (it == beacon_cache_.end()) return false;
    out = it->second;
    return true;
}

void Service::SetSelectedBeacon(uint32_t epoch, const Beacon& b)
{
    std::lock_guard<std::mutex> lock(mu_);
    beacon_cache_[epoch] = b;
}

RewardAccumulator& Service::CurrentAccumulator(uint32_t epoch)
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = accumulators_.find(epoch);
    if (it == accumulators_.end()) {
        it = accumulators_.emplace(epoch, RewardAccumulator(epoch)).first;
    }
    return it->second;
}

uint256 Service::CloseEpochAccumulator(uint32_t epoch)
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = accumulators_.find(epoch);
    if (it == accumulators_.end()) {
        return uint256(); // empty epoch
    }
    return it->second.CurrentRoot();
}

bool Service::IsStaleCoupled() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return stale_coupled_;
}

void Service::SetStaleCoupled(bool stale, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(mu_);
    stale_coupled_ = stale;
    stale_reason_ = reason;
}

// ----------------------------------------------------------------------------
// extern accessors declared in dace/kernel.cpp
// ----------------------------------------------------------------------------

const JointAnchor* GetActiveAnchorForStaking(int /*staking_height*/,
                                             const Consensus::Params& /*params*/)
{
    if (!Service::IsInitialized()) return nullptr;
    return Service::Get().Anchors().Active();
}

bool GetBeaconForStakingEpoch(uint32_t epoch,
                              const Consensus::Params& /*params*/,
                              Beacon& out)
{
    if (!Service::IsInitialized()) return false;
    return Service::Get().GetSelectedBeacon(epoch, out);
}

} // namespace dace
