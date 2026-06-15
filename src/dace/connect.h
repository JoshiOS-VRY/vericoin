// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_CONNECT_H
#define BITCOIN_DACE_CONNECT_H

#include <primitives/block.h>

class CBlockIndex;

namespace Consensus { struct Params; }

namespace dace {

/** Header-level DACE validation. Called from CheckBlockHeader for blocks at
 *  or above the activation height. Validates that the extended header fields
 *  are consistent with the current dace::Service state (active anchor,
 *  current epoch). Returns true if valid (or DACE inactive).
 *
 *  At height < activation this is a no-op returning true. */
bool CheckExtendedHeader(const CBlockHeader& header,
                         int height,
                         const Consensus::Params& params,
                         std::string& err);

/** ConnectBlock orchestration. Called near the end of ConnectBlock after
 *  the chain state has been updated. Drives:
 *
 *    - epoch boundary side-effects (close prev accumulator, open new one,
 *      record stale-coupling lag metrics)
 *    - reward accumulator updates from the block's coinbase / fees
 *    - anchor lifecycle promotion (Observed -> Certified -> Activated)
 *
 *  Pre-activation, this is a no-op. */
void OnConnectBlock(const CBlock& block,
                    const CBlockIndex* pindex,
                    const Consensus::Params& params);

/** Symmetric undo for OnConnectBlock. Called from DisconnectBlock. */
void OnDisconnectBlock(const CBlock& block,
                       const CBlockIndex* pindex,
                       const Consensus::Params& params);

} // namespace dace

#endif // BITCOIN_DACE_CONNECT_H
