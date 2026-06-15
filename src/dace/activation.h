// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_ACTIVATION_H
#define BITCOIN_DACE_ACTIVATION_H

#include <consensus/params.h>
#include <stdint.h>

/**
 * Binary Chain v3 (DACE) activation helpers.
 *
 * DACE consensus rules activate at a coordinated height on each chain:
 *   - VRC activation: consensus.BinaryChainHeightVRC
 *   - VRM activation: consensus.BinaryChainHeightVRM
 *
 * Below the activation height, legacy serialization and validation apply.
 * At or above the activation height, the extended header layout (DACE-1),
 * deterministic beacon (DACE-2), bonded ticket committee (DACE-3),
 * Joint Anchors (DACE-4), delayed reward accumulators (DACE-5), new P2P
 * messages (DACE-6), and stale-coupling IBD (DACE-7) all apply.
 *
 * A sentinel value of -1 (Consensus::Params::DaceNotScheduled) means DACE
 * is not scheduled for this network and legacy behavior is preserved
 * indefinitely.
 */
namespace dace {

/** Returns true if DACE rules are active at the given height on the current chain. */
bool IsActive(int nHeight, const Consensus::Params& params);

/** Returns true if DACE rules are active at the given height for a paired chain
 *  (consulted when validating cross-chain references). */
bool IsActiveForChain(int nHeight, const Consensus::Params& params, bool fIsVericoin);

/** Coupling epoch index for a VRC height. Undefined below activation. */
uint32_t EpochIndexVRC(int nHeight, const Consensus::Params& params);

/** Coupling epoch index for a VRM height. Undefined below activation. */
uint32_t EpochIndexVRM(int nHeight, const Consensus::Params& params);

} // namespace dace

#endif // BITCOIN_DACE_ACTIVATION_H
