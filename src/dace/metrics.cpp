// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/metrics.h>

#include <algorithm>
#include <vector>

namespace dace {

namespace {
constexpr size_t LAG_RING_SIZE = 1024;
} // namespace

Metrics& Metrics::Get()
{
    static Metrics instance;
    return instance;
}

void Metrics::RecordPairedHeaderLag(double seconds)
{
    std::lock_guard<std::mutex> lock(lag_mu_);
    lag_samples_.push_back(seconds);
    if (lag_samples_.size() > LAG_RING_SIZE) {
        lag_samples_.pop_front();
    }
}

void Metrics::PairedHeaderLagPercentiles(double& p50, double& p95, double& p99) const
{
    std::vector<double> samples;
    {
        std::lock_guard<std::mutex> lock(lag_mu_);
        samples.assign(lag_samples_.begin(), lag_samples_.end());
    }
    if (samples.empty()) {
        p50 = p95 = p99 = 0.0;
        return;
    }
    std::sort(samples.begin(), samples.end());
    auto pct = [&](double q) {
        size_t idx = std::min(samples.size() - 1,
                              static_cast<size_t>(q * samples.size()));
        return samples[idx];
    };
    p50 = pct(0.50);
    p95 = pct(0.95);
    p99 = pct(0.99);
}

} // namespace dace
