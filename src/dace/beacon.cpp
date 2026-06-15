// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/beacon.h>

#include <chain.h>
#include <consensus/params.h>
#include <pow.h>
#include <primitives/block.h>
#include <util/strencodings.h>

namespace dace {

bool SelectBeacon(uint32_t epoch,
                  const IPairedChainView& paired,
                  const Consensus::Params& params,
                  Beacon& out_beacon)
{
    // DACE-2: H_e = BinaryChainHeightVRM + e * DELTA.
    // First block at or after H_e that has K_BEACON confirmations on the
    // canonical paired chain is the candidate. If a reorg invalidated that
    // height before activation in a JA, walk the bounded fallback ladder.

    if (params.BinaryChainHeightVRM == Consensus::Params::DaceNotScheduled) {
        return false; // DACE not scheduled on the paired chain
    }
    const int64_t H_e = static_cast<int64_t>(params.BinaryChainHeightVRM)
                      + static_cast<int64_t>(epoch) * params.BeaconDelta;
    const int tip = paired.TipHeight();

    for (int offset = 0; offset <= params.BeaconFallbackWindow; ++offset) {
        const int64_t target = H_e + offset;
        if (target > tip) return false; // epoch undecided; paired chain too short

        const CBlockIndex* candidate = paired.AtHeight(static_cast<int>(target));
        if (candidate == nullptr) continue; // gap — try next offset

        const int depth = tip - candidate->nHeight;
        if (depth < params.BeaconK) continue; // not buried deeply enough; try next

        out_beacon.epoch_index = epoch;
        out_beacon.beacon_hash = candidate->GetBlockHash();
        out_beacon.beacon_height = static_cast<uint32_t>(candidate->nHeight);
        out_beacon.selection_offset = static_cast<uint32_t>(offset);
        return true;
    }
    return false; // ladder exhausted; epoch undecided
}

bool BuildDepthProof(const Beacon& beacon,
                     const IPairedChainView& paired,
                     const Consensus::Params& params,
                     BeaconDepthProof& out_proof,
                     std::string& err)
{
    const CBlockIndex* anchor = paired.AtHeight(static_cast<int>(beacon.beacon_height));
    if (!anchor || anchor->GetBlockHash() != beacon.beacon_hash) {
        err = "beacon block no longer on canonical paired chain";
        return false;
    }
    if (paired.TipHeight() - anchor->nHeight < params.BeaconK) {
        err = "insufficient depth on paired chain";
        return false;
    }

    out_proof.beacon_hash = beacon.beacon_hash;
    out_proof.beacon_height = beacon.beacon_height;
    out_proof.succeeding_headers.clear();
    out_proof.succeeding_headers.reserve(params.BeaconK);

    for (int h = anchor->nHeight + 1; h <= anchor->nHeight + params.BeaconK; ++h) {
        const CBlockIndex* succ = paired.AtHeight(h);
        if (!succ) {
            err = "missing succeeding header during proof build";
            return false;
        }
        out_proof.succeeding_headers.push_back(succ->GetBlockHeader());
    }
    return true;
}

bool BeaconDepthProof::Verify(const Consensus::Params& vrm_params,
                              std::string& err) const
{
    if (succeeding_headers.size() < static_cast<size_t>(vrm_params.BeaconK)) {
        err = "depth proof shorter than K_BEACON";
        return false;
    }
    uint256 prev_hash = beacon_hash;
    for (size_t i = 0; i < succeeding_headers.size(); ++i) {
        const CBlockHeader& h = succeeding_headers[i];
        if (h.hashPrevBlock != prev_hash) {
            err = "depth proof: hashPrevBlock chain break at index " + std::to_string(i);
            return false;
        }
        // PoW check: scrypt² (GetWorkHash) <= target derived from nBits.
        if (!CheckProofOfWork(h.GetWorkHash(), h.nBits, vrm_params)) {
            err = "depth proof: PoW fails at index " + std::to_string(i);
            return false;
        }
        prev_hash = h.GetHash();
    }
    return true;
}

} // namespace dace
