// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/reward_accumulator.h>

#include <consensus/merkle.h>
#include <hash.h>
#include <script/script.h>

namespace dace {

uint256 ClaimLeaf::GetLeafHash() const
{
    CHashWriter w(SER_GETHASH, 0);
    // Serialize the leaf canonically. We use the struct's own SerializationOp
    // through the CHashWriter << operator.
    w << *this;
    return w.GetHash();
}

void RewardAccumulator::AppendLeaf(const ClaimLeaf& leaf)
{
    leaves_.push_back(leaf);
}

uint256 RewardAccumulator::CurrentRoot() const
{
    std::vector<uint256> hashes;
    hashes.reserve(leaves_.size());
    for (const ClaimLeaf& l : leaves_) {
        hashes.push_back(l.GetLeafHash());
    }
    bool mutated = false;
    return ComputeMerkleRoot(hashes, &mutated);
}

bool RewardAccumulator::BuildProof(size_t leaf_index,
                                   std::vector<uint256>& out_path) const
{
    if (leaf_index >= leaves_.size()) return false;
    std::vector<uint256> hashes;
    hashes.reserve(leaves_.size());
    for (const ClaimLeaf& l : leaves_) hashes.push_back(l.GetLeafHash());
    out_path = ComputeMerkleBranch(hashes, static_cast<uint32_t>(leaf_index));
    return true;
}

CAmount DivertVrmSubsidy(CAmount base_subsidy, const Consensus::Params& params)
{
    // sigma is in basis points
    return (base_subsidy * params.DivertSigmaBpsVRM) / 10000;
}

CAmount DivertVrcFees(CAmount block_fees, const Consensus::Params& params)
{
    return (block_fees * params.DivertPhiBpsVRC) / 10000;
}

CScript GetEscrowScript(ClaimSourceChain chain)
{
    // The escrow scriptPubKey is a marker-style script that the chain's
    // consensus refuses to allow to be spent except via a valid claim
    // redemption transaction. The marker encodes the chain id so the script
    // also documents its purpose on-chain for any explorer.
    //
    // OP_RETURN-style would burn the funds; instead we use a non-standard but
    // valid script: OP_DEPTH 0 OP_EQUAL <chain_id> OP_DROP <DACE_ESCROW_TAG>
    // which is consensus-handled by the claim redemption rule. For Phase 1
    // we use a simple OP_RESERVED-prefixed bag that can only be spent by a
    // transaction whose witness deserializes as a ClaimRedemptionWitness.
    const uint8_t tag[5] = {0x44, 0x41, 0x43, 0x45, 0x45}; // "DACEE"
    CScript s;
    s << std::vector<unsigned char>(tag, tag + sizeof(tag));
    s << static_cast<uint8_t>(chain);
    s << OP_DROP;
    s << OP_DROP;
    s << OP_TRUE;  // base validity; the consensus claim rule overrides this
    return s;
}

} // namespace dace
