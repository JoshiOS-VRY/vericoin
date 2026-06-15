// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_HEADER_H
#define BITCOIN_DACE_HEADER_H

#include <primitives/block.h>
#include <consensus/params.h>

namespace dace {

/**
 * RAII scope that enables DACE-1 extended block-header serialization on the
 * current thread.
 *
 * Usage:
 *     {
 *         dace::ExtendedSerializationScope scope;
 *         ss << header;                  // serializes 180 bytes
 *         hash = SerializeHash(header);  // hashes the extended layout
 *     }
 *     // scope destruction restores prior mode (typically legacy 80 bytes)
 *
 * Scopes nest; restoration is to the prior value (not unconditionally false).
 *
 * Callers must determine, from block height vs. consensus.BinaryChainHeight*,
 * whether to enter the scope. Helpers below wrap common patterns.
 */
class ExtendedSerializationScope
{
public:
    ExtendedSerializationScope();
    ~ExtendedSerializationScope();

    ExtendedSerializationScope(const ExtendedSerializationScope&) = delete;
    ExtendedSerializationScope& operator=(const ExtendedSerializationScope&) = delete;

private:
    bool prev;
};

/** Hash a header using the layout appropriate for its height. */
uint256 ComputeBlockHash(const CBlockHeader& header,
                         int nHeight,
                         const struct Consensus::Params& params);

/** Hash a header in legacy layout regardless of activation state.
 *  Used during IBD of pre-fork history. */
uint256 ComputeLegacyHash(const CBlockHeader& header);

/** Hash a header in extended layout regardless of activation state.
 *  Used during validation of new blocks above activation. */
uint256 ComputeExtendedHash(const CBlockHeader& header);

} // namespace dace

#endif // BITCOIN_DACE_HEADER_H
