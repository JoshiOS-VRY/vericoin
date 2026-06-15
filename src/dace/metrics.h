// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_METRICS_H
#define BITCOIN_DACE_METRICS_H

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <stdint.h>
#include <string>

namespace dace {

/** Threat-model metrics. See design guide Section 13.1 + DACE-7 §"Stale-coupling".
 *
 *  All counters are monotonic. Histograms are bounded ring buffers (last N
 *  samples) so reading them via RPC is bounded in size.
 */
class Metrics {
public:
    static Metrics& Get();

    // Counters
    std::atomic<uint64_t> divergent_beacon_selections{0};
    std::atomic<uint64_t> reorgs_at_beacon{0};
    std::atomic<uint64_t> committee_missed_votes{0};
    std::atomic<uint64_t> foreign_payee_rejections{0};
    std::atomic<uint64_t> consecutive_missed_anchors{0};
    std::atomic<uint64_t> ibd_replay_consensus_diffs{0};
    std::atomic<uint64_t> stale_coupling_entries{0};
    std::atomic<uint64_t> recovery_anchors_built{0};
    std::atomic<uint64_t> recovery_anchors_activated{0};
    std::atomic<uint64_t> claim_redemptions_total{0};
    std::atomic<uint64_t> claim_redemptions_rejected{0};
    std::atomic<uint64_t> slashes_total{0};

    /** Record a paired-header lag sample (in seconds). Bounded ring of last
     *  1024 samples. */
    void RecordPairedHeaderLag(double seconds);

    /** p50/p95/p99 of paired header lag samples. */
    void PairedHeaderLagPercentiles(double& p50, double& p95, double& p99) const;

    /** Concentration: top-10 ticket-share fraction at last sampled epoch.
     *  Updated by ConnectBlock at each epoch boundary. */
    std::atomic<double> top10_ticket_share{0.0};
    /** Concentration: top-N seat-share divided by top-N bonded-share. >1.0
     *  indicates superlinear gain from splitting (alarm-worthy). */
    std::atomic<double> seat_to_bonded_ratio_top10{1.0};

private:
    Metrics() = default;

    mutable std::mutex lag_mu_;
    std::deque<double> lag_samples_;
};

} // namespace dace

#endif // BITCOIN_DACE_METRICS_H
