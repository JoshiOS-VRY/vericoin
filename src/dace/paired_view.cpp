// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/paired_view.h>

#include <util/memory.h>

namespace dace {

void PairedHeaderStore::IngestHeaders(const std::vector<CBlockHeader>& headers)
{
    if (headers.empty()) return;
    std::lock_guard<std::mutex> lock(mu_);

    for (const CBlockHeader& h : headers) {
        const uint256 hash = h.GetHash();
        if (by_hash_.find(hash) != by_hash_.end()) {
            continue; // already known
        }

        // Determine height by chaining onto a known prev. If prev is
        // unknown, assume this header starts a sequence anchored to the
        // current tip height + 1.
        int height = tip_height_ + 1;
        auto prev_it = by_hash_.find(h.hashPrevBlock);
        if (prev_it != by_hash_.end()) {
            height = prev_it->second->nHeight + 1;
        }

        auto idx = MakeUnique<CBlockIndex>(h);
        idx->nHeight = height;
        idx->phashBlock = nullptr;
        CBlockIndex* raw = idx.get();
        by_hash_[hash] = std::move(idx);

        // Active-chain bookkeeping: only the longest contiguous chain is
        // exposed via AtHeight. If the new header extends the current tip,
        // accept it as the new tip. Forks at lower heights are stored but
        // not active.
        if (height > tip_height_) {
            by_height_[height] = raw;
            tip_height_ = height;
        }
    }
}

void PairedHeaderStore::TrimAbove(int keep_tip_height)
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = by_height_.upper_bound(keep_tip_height);
    while (it != by_height_.end()) {
        // Note: we leave by_hash_ entries intact (cheap fork retention)
        // and only prune the active-chain mapping.
        it = by_height_.erase(it);
    }
    tip_height_ = keep_tip_height;
}

int PairedHeaderStore::TipHeight() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return tip_height_;
}

const CBlockIndex* PairedHeaderStore::AtHeight(int height) const
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = by_height_.find(height);
    return it == by_height_.end() ? nullptr : it->second;
}

} // namespace dace
