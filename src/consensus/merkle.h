// Copyright (c) 2015-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_MERKLE_H
#define BITCOIN_CONSENSUS_MERKLE_H

#include <vector>

#include <primitives/block.h>
#include <uint256.h>

uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated = nullptr);

/** Reconstruct a merkle root from a leaf hash, sibling path, and leaf index. */
uint256 ComputeMerkleRootFromBranch(const uint256& leaf,
                                    const std::vector<uint256>& merkle_branch,
                                    uint32_t index);

/** Build a merkle inclusion proof for the leaf at `position`. */
std::vector<uint256> ComputeMerkleBranch(const std::vector<uint256>& leaves,
                                         uint32_t position);

/*
 * Compute the Merkle root of the transactions in a block.
 * *mutated is set to true if a duplicated subtree was found.
 */
uint256 BlockMerkleRoot(const CBlock& block, bool* mutated = nullptr);

/*
 * Compute the Merkle root of the witness transactions in a block.
 * *mutated is set to true if a duplicated subtree was found.
 */
uint256 BlockWitnessMerkleRoot(const CBlock& block, bool* mutated = nullptr);

#endif // BITCOIN_CONSENSUS_MERKLE_H
