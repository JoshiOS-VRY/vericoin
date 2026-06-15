// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_KERNEL_H
#define BITCOIN_DACE_KERNEL_H

#include <dace/joint_anchor.h>
#include <consensus/params.h>
#include <uint256.h>

#include <stdint.h>

namespace dace {

/** PoSTaP v2 kernel input mixer per DACE design guide Section 6.
 *
 *  Legacy (pre-activation): returns the input stake modifier unchanged.
 *  DACE-active: returns a 256-bit "modMix" derived from
 *      modMix = SHA256d( legacyStakeModifier (uint64 LE) ||
 *                        beacon.GetWorkHash() ||
 *                        beacon.hashCrossChain ||
 *                        nBits LE32 ||
 *                        nTime LE32 ||
 *                        epoch_index LE32 )
 *
 *  Returning a 256-bit mix lets the kernel SerializationOp consume it as a
 *  full uint256, replacing the legacy uint64 modifier where DACE is active.
 *  Callers must serialize differently based on IsActive.
 *
 *  Note: the kernel itself in pos.cpp serializes either 8 bytes (legacy
 *  uint64 nStakeModifier) or 32 bytes (DACE modMix) followed by the same
 *  trailing fields (nTimeBlockFrom, nTxPrevOffset, txPrevTime, prevout.n,
 *  nTimeTx). This change is intentional and gated by activation.
 */

/** Result of the DACE kernel mix: either legacy (uint64) or DACE (uint256). */
struct KernelMix {
    bool dace_active{false};
    uint64_t legacy_modifier{0};   // valid when dace_active = false
    uint256 dace_modifier;          // valid when dace_active = true
};

/** Compute the kernel mix for a staking attempt whose owning block would be
 *  at staking_height on the local chain.
 *
 *  @param staking_height  height the new stake block would have
 *  @param legacy_modifier classic ppcoin stake modifier (from
 *                         GetKernelStakeModifier)
 *  @param params          consensus params (must include DACE constants and
 *                         activation height)
 *  @param out             populated
 *  @return                true on success; false only when DACE is active and
 *                         the required JA_active + beacon are unavailable
 *                         (caller should defer staking — stale-coupling)
 */
bool ComputeKernelMix(int staking_height,
                      uint64_t legacy_modifier,
                      unsigned int nBits,
                      unsigned int nTime,
                      const Consensus::Params& params,
                      KernelMix& out);

} // namespace dace

#endif // BITCOIN_DACE_KERNEL_H
