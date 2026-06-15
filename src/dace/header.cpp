// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/header.h>

#include <dace/activation.h>
#include <crypto/scrypt.h>
#include <hash.h>

namespace {

// Thread-local flag toggled by ExtendedSerializationScope. Header
// SerializationOp consults dace_block_extended_serialization_enabled().
thread_local bool g_dace_extended_serialization{false};

} // namespace

// Free function called from primitives/block.h SerializationOp.
bool dace_block_extended_serialization_enabled()
{
    return g_dace_extended_serialization;
}

namespace dace {

ExtendedSerializationScope::ExtendedSerializationScope()
    : prev(g_dace_extended_serialization)
{
    g_dace_extended_serialization = true;
}

ExtendedSerializationScope::~ExtendedSerializationScope()
{
    g_dace_extended_serialization = prev;
}

uint256 ComputeBlockHash(const CBlockHeader& header,
                         int nHeight,
                         const Consensus::Params& params)
{
    if (IsActive(nHeight, params)) {
        return ComputeExtendedHash(header);
    }
    return ComputeLegacyHash(header);
}

uint256 ComputeLegacyHash(const CBlockHeader& header)
{
    // Legacy: extended-serialization scope OFF, follows the existing
    // GetVeriumHash / GetWorkHash dispatch path in block.cpp.
    // Caller-side scope guarantees no extended fields are hashed.
    // (We just call GetHash; primitives/block.cpp dispatches to the right
    // hash function for VRC vs VRM.)
    return header.GetHash();
}

uint256 ComputeExtendedHash(const CBlockHeader& header)
{
    ExtendedSerializationScope scope;
    return header.GetHash();
}

} // namespace dace
