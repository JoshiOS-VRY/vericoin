// Copyright (c) 2015-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/merkle.h>
#include <hash.h>

/*     WARNING! If you're reading this because you're learning about crypto
       and/or designing a new system that will use merkle trees, keep in mind
       that the following merkle tree algorithm has a serious flaw related to
       duplicate txids, resulting in a vulnerability (CVE-2012-2459).

       The reason is that if the number of hashes in the list at a given level
       is odd, the last one is duplicated before computing the next level (which
       is unusual in Merkle trees). This results in certain sequences of
       transactions leading to the same merkle root. For example, these two
       trees:

                    A               A
                  /  \            /   \
                B     C         B       C
               / \    |        / \     / \
              D   E   F       D   E   F   F
             / \ / \ / \     / \ / \ / \ / \
             1 2 3 4 5 6     1 2 3 4 5 6 5 6

       for transaction lists [1,2,3,4,5,6] and [1,2,3,4,5,6,5,6] (where 5 and
       6 are repeated) result in the same root hash A (because the hash of both
       of (F) and (F,F) is C).

       The vulnerability results from being able to send a block with such a
       transaction list, with the same merkle root, and the same block hash as
       the original without duplication, resulting in failed validation. If the
       receiving node proceeds to mark that block as permanently invalid
       however, it will fail to accept further unmodified (and thus potentially
       valid) versions of the same block. We defend against this by detecting
       the case where we would hash two identical hashes at the end of the list
       together, and treating that identically to the block having an invalid
       merkle root. Assuming no double-SHA256 collisions, this will detect all
       known ways of changing the transactions without affecting the merkle
       root.
*/


uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated) {
    bool mutation = false;
    while (hashes.size() > 1) {
        if (mutated) {
            for (size_t pos = 0; pos + 1 < hashes.size(); pos += 2) {
                if (hashes[pos] == hashes[pos + 1]) mutation = true;
            }
        }
        if (hashes.size() & 1) {
            hashes.push_back(hashes.back());
        }
        SHA256D64(hashes[0].begin(), hashes[0].begin(), hashes.size() / 2);
        hashes.resize(hashes.size() / 2);
    }
    if (mutated) *mutated = mutation;
    if (hashes.size() == 0) return uint256();
    return hashes[0];
}

uint256 ComputeMerkleRootFromBranch(const uint256& leaf,
                                    const std::vector<uint256>& merkle_branch,
                                    uint32_t index)
{
    uint256 hash = leaf;
    for (const uint256& branch : merkle_branch) {
        if (index & 1) {
            hash = Hash(branch.begin(), branch.end(), hash.begin(), hash.end());
        } else {
            hash = Hash(hash.begin(), hash.end(), branch.begin(), branch.end());
        }
        index >>= 1;
    }
    return hash;
}

namespace {

/** Constant-space merkle root/path calculator (Bitcoin Core test helper). */
void MerkleComputation(const std::vector<uint256>& leaves,
                       uint256* proot,
                       bool* pmutated,
                       uint32_t branchpos,
                       std::vector<uint256>* pbranch)
{
    if (pbranch) pbranch->clear();
    if (leaves.size() == 0) {
        if (pmutated) *pmutated = false;
        if (proot) *proot = uint256();
        return;
    }
    bool mutated = false;
    uint32_t count = 0;
    uint256 inner[32];
    int matchlevel = -1;
    while (count < leaves.size()) {
        uint256 h = leaves[count];
        bool matchh = count == branchpos;
        count++;
        int level;
        for (level = 0; !(count & (((uint32_t)1) << level)); level++) {
            if (pbranch) {
                if (matchh) {
                    pbranch->push_back(inner[level]);
                } else if (matchlevel == level) {
                    pbranch->push_back(h);
                    matchh = true;
                }
            }
            mutated |= (inner[level] == h);
            CHash256().Write(inner[level].begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
        }
        inner[level] = h;
        if (matchh) {
            matchlevel = level;
        }
    }
    int level = 0;
    while (!(count & (((uint32_t)1) << level))) {
        level++;
    }
    uint256 h = inner[level];
    bool matchh = matchlevel == level;
    while (count != (((uint32_t)1) << level)) {
        if (pbranch && matchh) {
            pbranch->push_back(h);
        }
        CHash256().Write(h.begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
        count += (((uint32_t)1) << level);
        level++;
        while (!(count & (((uint32_t)1) << level))) {
            if (pbranch) {
                if (matchh) {
                    pbranch->push_back(inner[level]);
                } else if (matchlevel == level) {
                    pbranch->push_back(h);
                    matchh = true;
                }
            }
            CHash256().Write(inner[level].begin(), 32).Write(h.begin(), 32).Finalize(h.begin());
            level++;
        }
    }
    if (pmutated) *pmutated = mutated;
    if (proot) *proot = h;
}

} // namespace

std::vector<uint256> ComputeMerkleBranch(const std::vector<uint256>& leaves,
                                         uint32_t position)
{
    std::vector<uint256> ret;
    MerkleComputation(leaves, nullptr, nullptr, position, &ret);
    return ret;
}


uint256 BlockMerkleRoot(const CBlock& block, bool* mutated)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    for (size_t s = 0; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s]->GetHash();
    }
    return ComputeMerkleRoot(std::move(leaves), mutated);
}

uint256 BlockWitnessMerkleRoot(const CBlock& block, bool* mutated)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    leaves[0].SetNull(); // The witness hash of the coinbase is 0.
    for (size_t s = 1; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s]->GetWitnessHash();
    }
    return ComputeMerkleRoot(std::move(leaves), mutated);
}

