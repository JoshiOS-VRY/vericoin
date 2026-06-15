// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/activation.h>

#include <chainparams.h>
#include <consensus/params.h>

namespace dace {

bool IsActive(int nHeight, const Consensus::Params& params)
{
    if (params.fIsVericoin) {
        return params.BinaryChainHeightVRC != Consensus::Params::DaceNotScheduled
            && nHeight >= params.BinaryChainHeightVRC;
    } else {
        return params.BinaryChainHeightVRM != Consensus::Params::DaceNotScheduled
            && nHeight >= params.BinaryChainHeightVRM;
    }
}

bool IsActiveForChain(int nHeight, const Consensus::Params& params, bool fIsVericoin)
{
    if (fIsVericoin) {
        return params.BinaryChainHeightVRC != Consensus::Params::DaceNotScheduled
            && nHeight >= params.BinaryChainHeightVRC;
    } else {
        return params.BinaryChainHeightVRM != Consensus::Params::DaceNotScheduled
            && nHeight >= params.BinaryChainHeightVRM;
    }
}

uint32_t EpochIndexVRC(int nHeight, const Consensus::Params& params)
{
    if (params.BinaryChainHeightVRC == Consensus::Params::DaceNotScheduled
        || nHeight < params.BinaryChainHeightVRC) {
        return 0;
    }
    return static_cast<uint32_t>(
        (nHeight - params.BinaryChainHeightVRC) / params.BeaconEpochVRC);
}

uint32_t EpochIndexVRM(int nHeight, const Consensus::Params& params)
{
    if (params.BinaryChainHeightVRM == Consensus::Params::DaceNotScheduled
        || nHeight < params.BinaryChainHeightVRM) {
        return 0;
    }
    return static_cast<uint32_t>(
        (nHeight - params.BinaryChainHeightVRM) / params.BeaconDelta);
}

} // namespace dace
