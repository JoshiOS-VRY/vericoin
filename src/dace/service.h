// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_SERVICE_H
#define BITCOIN_DACE_SERVICE_H

#include <dace/anchor_lifecycle.h>
#include <dace/beacon.h>
#include <dace/claim.h>
#include <dace/paired_view.h>
#include <dace/reward_accumulator.h>
#include <dace/ticket_registry.h>

#include <memory>
#include <mutex>

namespace dace {

/** DACE service singleton — owns the cross-chain state that needs to outlive
 *  individual block-processing calls. Lifecycle:
 *
 *    AppInitMain (init.cpp):  dace::Service::Initialize(params, datadir);
 *    Shutdown (init.cpp):     dace::Service::Teardown();
 *
 *  Inside block validation, code accesses dace::Service::Get() to read or
 *  mutate the active anchor, ticket registry, beacons, accumulators, and
 *  claim-spent set.
 */
class Service {
public:
    static void Initialize(const Consensus::Params& params,
                           const std::string& datadir);
    static void Teardown();
    static Service& Get();
    static bool IsInitialized();

    // ---- Anchor lifecycle accessors (used during ConnectBlock) ----
    AnchorLifecycle& Anchors() { return anchors_; }
    const AnchorLifecycle& Anchors() const { return anchors_; }

    // ---- Ticket registry accessors ----
    TicketRegistry& Tickets() { return tickets_; }
    const TicketRegistry& Tickets() const { return tickets_; }

    // ---- Beacon cache ----
    /** Look up the selected beacon for an epoch from the local cache.
     *  Returns false if the beacon has not yet been selected. */
    bool GetSelectedBeacon(uint32_t epoch, Beacon& out) const;
    /** Record that this epoch's beacon is `b` (called when a block at the
     *  start of a new epoch finalizes selection). */
    void SetSelectedBeacon(uint32_t epoch, const Beacon& b);

    // ---- Reward accumulators ----
    /** Get the current epoch's accumulator for the local chain. */
    RewardAccumulator& CurrentAccumulator(uint32_t epoch);
    /** Mark the current epoch closed and return the finalized root. */
    uint256 CloseEpochAccumulator(uint32_t epoch);

    // ---- Claim-spent set ----
    ClaimSpentSet& Claims() { return claims_; }
    const ClaimSpentSet& Claims() const { return claims_; }

    // ---- Stale-coupling status (read by RPC binarychain_status) ----
    bool IsStaleCoupled() const;
    void SetStaleCoupled(bool stale, const std::string& reason);

    // ---- Paired chain header store (populated by net_processing) ----
    PairedHeaderStore& PairedHeaders() { return paired_; }
    const PairedHeaderStore& PairedHeaders() const { return paired_; }

private:
    Service() = default;

    AnchorLifecycle anchors_;
    TicketRegistry tickets_;
    std::map<uint32_t, Beacon> beacon_cache_;
    std::map<uint32_t, RewardAccumulator> accumulators_;
    ClaimSpentSet claims_;
    PairedHeaderStore paired_;

    bool stale_coupled_{false};
    std::string stale_reason_;

    mutable std::mutex mu_;
};

} // namespace dace

#endif // BITCOIN_DACE_SERVICE_H
