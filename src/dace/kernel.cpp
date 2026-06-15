// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/kernel.h>

#include <dace/activation.h>
#include <dace/anchor_lifecycle.h>
#include <dace/beacon.h>
#include <hash.h>

namespace dace {

// External singleton accessors (defined in dace/service.cpp). Keeping these
// here as extern declarations avoids a dependency cycle between kernel.cpp
// and the higher-level service module.
extern const JointAnchor* GetActiveAnchorForStaking(int staking_height,
                                                    const Consensus::Params& params);
extern bool GetBeaconForStakingEpoch(uint32_t epoch,
                                     const Consensus::Params& params,
                                     Beacon& out);

bool ComputeKernelMix(int staking_height,
                      uint64_t legacy_modifier,
                      unsigned int nBits,
                      unsigned int nTime,
                      const Consensus::Params& params,
                      KernelMix& out)
{
    out.dace_active = IsActive(staking_height, params);
    if (!out.dace_active) {
        out.legacy_modifier = legacy_modifier;
        return true;
    }

    const uint32_t epoch = EpochIndexVRC(staking_height, params);

    // We require both an active anchor and a selected beacon for this epoch.
    const JointAnchor* ja_active = GetActiveAnchorForStaking(staking_height, params);
    Beacon beacon;
    if (ja_active == nullptr || !GetBeaconForStakingEpoch(epoch, params, beacon)) {
        return false; // defer staking; stale-coupling
    }

    // The "cross-chain commitment" of the beacon is the beacon block header's
    // pairedAnchorRef (DACE-1). In a paired-chain-stored header set, this is
    // available alongside the beacon block.
    //
    // We cannot read VRM block headers directly from this layer without a
    // PairedChainView; the service module already cached the relevant header
    // and exposes it via beacon_cross_chain accessor. To keep this file free
    // of cycle-creating includes, we rely on the beacon hash itself as the
    // primary entropy source for Phase 1 and leave the cross-chain commitment
    // wiring to follow-up work.
    //
    // Phase 1 mix: modMix = H( legacy_modifier_le8 || beacon_hash || nBits ||
    //                          nTime || epoch_index )
    // Phase 2 mix (future): include beacon.hashCrossChain in the input.

    CHashWriter w(SER_GETHASH, 0);
    uint64_t mod_le = legacy_modifier;
    w.write(reinterpret_cast<const char*>(&mod_le), 8);
    w.write(reinterpret_cast<const char*>(beacon.beacon_hash.begin()), 32);
    w.write(reinterpret_cast<const char*>(&nBits), 4);
    w.write(reinterpret_cast<const char*>(&nTime), 4);
    w.write(reinterpret_cast<const char*>(&epoch), 4);
    out.dace_modifier = w.GetHash();
    return true;
}

} // namespace dace
