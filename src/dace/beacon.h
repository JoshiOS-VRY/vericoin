// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_BEACON_H
#define BITCOIN_DACE_BEACON_H

#include <consensus/params.h>
#include <primitives/block.h>
#include <serialize.h>
#include <uint256.h>

#include <vector>

class CBlockIndex;

namespace dace {

/** A selected paired-chain (VRM) beacon for a coupling epoch. See DACE-2. */
struct Beacon {
    uint32_t epoch_index{0};
    uint256  beacon_hash;
    uint32_t beacon_height{0};
    uint32_t selection_offset{0}; ///< fallback-ladder offset used (0 = exact H_e)

    bool IsNull() const { return beacon_hash.IsNull(); }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(epoch_index);
        READWRITE(beacon_hash);
        READWRITE(beacon_height);
        READWRITE(selection_offset);
    }
};

/** Depth proof attached to a beacon for relay to peers that don't yet have the
 *  paired chain headers. K_BEACON consecutive headers immediately following
 *  the beacon block, building deterministically on its hash. */
struct BeaconDepthProof {
    uint256 beacon_hash;
    uint32_t beacon_height{0};
    std::vector<CBlockHeader> succeeding_headers;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(beacon_hash);
        READWRITE(beacon_height);
        READWRITE(succeeding_headers);
    }

    /** Verify that succeeding_headers form a chain building on beacon_hash
     *  with valid PoW, and that depth meets params.BeaconK. */
    bool Verify(const Consensus::Params& vrm_params, std::string& err) const;
};

/** Interface used by beacon selection to query the paired-chain header set
 *  (the VRM chain when called from a VRC daemon, or the VRC chain when called
 *  from a VRM daemon for cross-checks). */
class IPairedChainView {
public:
    virtual ~IPairedChainView() = default;
    /** Tip height of the canonical paired chain. */
    virtual int TipHeight() const = 0;
    /** Block index at a specific height on the canonical paired chain, or
     *  nullptr if not present. */
    virtual const CBlockIndex* AtHeight(int height) const = 0;
};

/** Select the beacon for coupling epoch `e` per DACE-2.
 *
 *  @param epoch       coupling epoch index (e)
 *  @param paired      view over the paired chain (VRM when called from VRC)
 *  @param params      consensus params containing BeaconDelta / BeaconK /
 *                      BeaconFallbackWindow / BinaryChainHeightVRM
 *  @param out_beacon  populated on success
 *  @return            true if a beacon could be selected and meets the
 *                      K_BEACON confirmation requirement; false otherwise
 *                      (epoch undecided — stale-coupling triggers).
 */
bool SelectBeacon(uint32_t epoch,
                  const IPairedChainView& paired,
                  const Consensus::Params& params,
                  Beacon& out_beacon);

/** Build a BeaconDepthProof for a selected beacon. Caller-supplied paired view
 *  must include succeeding_headers up to beacon_height + params.BeaconK. */
bool BuildDepthProof(const Beacon& beacon,
                     const IPairedChainView& paired,
                     const Consensus::Params& params,
                     BeaconDepthProof& out_proof,
                     std::string& err);

} // namespace dace

#endif // BITCOIN_DACE_BEACON_H
