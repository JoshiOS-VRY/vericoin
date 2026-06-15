// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <arith_uint256.h>
#include <uint256.h>
#include <limits>

namespace Consensus {

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which CSV (BIP68, BIP112 and BIP113) becomes active */
    int CSVHeight;

    bool fIsVericoin;

    /** Vericoin Params **/
    uint256 posLimit;
    int NextTargetV2Height;
    int PoSTHeight;
    int PoSHeight;
    int nStakeTargetSpacing;
    int nStakeMinAge;
    int nModifierInterval;

    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;

    /** coin params **/
    int nMaturity;
    int nTargetTimespan;
    int nInitialCoinSupply;
    int VIP1Height;

    /** ------------------------------------------------------------------------
     *  Binary Chain v3 (DACE) parameters. See vericoin/doc/dace/DACE-0.
     *  All defaults reflect the Balanced profile in DACE-0 Section "Frozen
     *  consensus constants". Set BinaryChainHeight* to DaceNotScheduled to
     *  preserve legacy behavior indefinitely.
     *  ------------------------------------------------------------------------ */
    static constexpr int DaceNotScheduled = -1;

    /** Activation heights (chain-specific). One of these is consulted depending
     *  on whether the running daemon is vericoind or veriumd. */
    int BinaryChainHeightVRC{DaceNotScheduled};
    int BinaryChainHeightVRM{DaceNotScheduled};

    /** Bootstrap anchors shipped at activation. Hash of the last pre-fork header
     *  on each chain plus the hash of the initial Joint Anchor JA_0. */
    uint256 DaceBootstrapHeaderVRC;
    uint256 DaceBootstrapHeaderVRM;
    uint256 DaceBootstrapJAGenesis;

    /** Beacon parameters (DACE-2). */
    int BeaconDelta{12};                // VRM block stride between beacons
    int BeaconK{50};                    // confirmations required to qualify
    int BeaconFallbackWindow{6};        // bounded deterministic fallback ladder
    int BeaconEpochVRC{60};             // VRC blocks per coupling epoch

    /** Bonded ticket registry (DACE-3). */
    int64_t TicketStakeUnit{1000 * 100000000LL}; // 1000 VRC in satoshi-equivalents
    int TicketLockupEpochs{6};                   // L
    int TicketUnbondDelayEpochs{2};

    /** Committee (DACE-3). */
    int CommitteeSize{128};               // M
    int CommitteeQuorumNumerator{2};
    int CommitteeQuorumDenominator{3};

    /** Finality and recovery (DACE-4). */
    int StaleGraceEpochs{3};
    int StaleMaxEpochs{16};               // S_MAX
    int FinalityWindowSeconds{6 * 60 * 60};
    int RecoveryThresholdNumerator{4};    // 4/5 = 0.80
    int RecoveryThresholdDenominator{5};
    int FinalityGraceBlocks{18};
    int CoupleLookaheadEpochs{5};

    /** Economic divert bands (DACE-5). Expressed in basis points (1 / 10000). */
    int DivertSigmaBpsVRM{400};           // sigma = 4% of VRM subsidy
    int DivertPhiBpsVRC{1000};            // phi   = 10% of VRC fees

    /** Claim lifecycle (DACE-5). */
    int ClaimExpiryEpochs{1024};

    /** Networking (DACE-6). */
    int BcProtocolVersion{90100};
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
