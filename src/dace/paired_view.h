// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_PAIRED_VIEW_H
#define BITCOIN_DACE_PAIRED_VIEW_H

#include <chain.h>
#include <dace/beacon.h>
#include <primitives/block.h>
#include <uint256.h>

#include <map>
#include <memory>
#include <mutex>

namespace dace {

/** In-memory store of paired-chain headers received via the `xheaders` P2P
 *  message. A single instance is owned by dace::Service and consulted by
 *  beacon selection / Joint Anchor validation.
 *
 *  Owns CBlockIndex instances synthesized from received CBlockHeaders so the
 *  beacon API (which takes `const CBlockIndex*`) works uniformly between the
 *  local chain and the paired chain.
 */
class PairedHeaderStore : public IPairedChainView {
public:
    PairedHeaderStore() = default;

    /** Insert a contiguous sequence of headers received from a peer. The
     *  store auto-derives heights by chaining from previously stored entries
     *  (or treating the first header as height 0 if nothing exists yet).
     *  Idempotent for already-known headers. */
    void IngestHeaders(const std::vector<CBlockHeader>& headers);

    /** Remove all headers above `keep_tip_height` (e.g., for reorg
     *  reconciliation). */
    void TrimAbove(int keep_tip_height);

    // ---- IPairedChainView ----
    int TipHeight() const override;
    const CBlockIndex* AtHeight(int height) const override;

private:
    mutable std::mutex mu_;
    // Owned CBlockIndex objects keyed by header hash.
    std::map<uint256, std::unique_ptr<CBlockIndex>> by_hash_;
    // Active chain (height -> index pointer). Updated as headers extend.
    std::map<int, CBlockIndex*> by_height_;
    int tip_height_{-1};
};

} // namespace dace

#endif // BITCOIN_DACE_PAIRED_VIEW_H
