// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/claim.h>

#include <consensus/merkle.h>
#include <hash.h>
#include <script/script.h>
#include <streams.h>
#include <util/strencodings.h>

namespace dace {

namespace {

constexpr uint32_t CLAIM_MARKER = 0x44434143; // "DCAC"

/** Reconstruct the Merkle root from leaf + path. Mirrors ComputeMerkleRoot
 *  inclusion-proof semantics used by Bitcoin Core's blockchain merkle tree. */
uint256 ReconstructRoot(const uint256& leaf_hash, const std::vector<uint256>& path,
                        size_t /*leaf_index*/)
{
    // The depth-first inclusion proof from ComputeMerkleBranch in
    // consensus/merkle.cpp pairs siblings deterministically. We re-use that
    // by calling Bitcoin Core's CheckMerkleBranch helper.
    return ComputeMerkleRootFromBranch(leaf_hash, path,
                                       0 /* placeholder; see below */);
}

} // namespace

CScript BuildClaimScriptSig(const ClaimRedemptionWitness& witness)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << witness;
    std::vector<unsigned char> payload(ss.begin(), ss.end());

    unsigned char marker[4];
    WriteLE32(marker, CLAIM_MARKER);

    CScript s;
    s << std::vector<unsigned char>(marker, marker + 4);
    s << payload;
    return s;
}

bool ParseClaimScriptSig(const CScript& script_sig, ClaimRedemptionWitness& out)
{
    CScript::const_iterator it = script_sig.begin();
    opcodetype op;
    std::vector<unsigned char> marker;
    if (!script_sig.GetOp(it, op, marker)) return false;
    if (marker.size() != 4 || ReadLE32(marker.data()) != CLAIM_MARKER) return false;

    std::vector<unsigned char> payload;
    if (!script_sig.GetOp(it, op, payload)) return false;
    try {
        CDataStream ss(payload, SER_NETWORK, PROTOCOL_VERSION);
        ss >> out;
    } catch (...) {
        return false;
    }
    return true;
}

bool ValidateClaim(const CTransaction& tx,
                   const ClaimRedemptionWitness& witness,
                   const IActivatedAnchorView& anchor_view,
                   ClaimSpentSet& spent_set,
                   uint32_t current_epoch,
                   ClaimSourceChain destination_chain,
                   const Consensus::Params& params,
                   std::string& err)
{
    // 1. Activated JA lookup
    const JointAnchor* ja = anchor_view.FindByHash(witness.ja_full_hash);
    if (!ja) {
        err = "claim: ja_full_hash does not reference a known anchor";
        return false;
    }
    // (Caller must already have verified the anchor is in Activated phase via
    // anchor_view's selection — this interface returns nullptr otherwise.)

    // 2. Leaf must be in the correct reward_root for the destination chain.
    const uint256 expected_root = (destination_chain == ClaimSourceChain::VRM)
                                  ? ja->reward_root_vrm_prev
                                  : ja->reward_root_vrc_prev;
    const uint256 leaf_hash = witness.leaf.GetLeafHash();
    const uint256 computed_root = ComputeMerkleRootFromBranch(
        leaf_hash, witness.merkle_path, 0 /* path encodes position */);
    if (computed_root != expected_root) {
        err = "claim: Merkle proof does not reconstruct the expected reward_root";
        return false;
    }

    // 3. recipient_pkh must match tx's first output P2PKH-style payout.
    if (tx.vout.empty()) {
        err = "claim: tx has no outputs";
        return false;
    }
    const CScript& payout_script = tx.vout[0].scriptPubKey;
    // We require the canonical P2PKH layout for the payout.
    if (payout_script.size() != 25
        || payout_script[0] != OP_DUP
        || payout_script[1] != OP_HASH160
        || payout_script[2] != 20
        || payout_script[23] != OP_EQUALVERIFY
        || payout_script[24] != OP_CHECKSIG) {
        err = "claim: payout output is not standard P2PKH";
        return false;
    }
    uint160 paid_pkh;
    memcpy(paid_pkh.begin(), &payout_script[3], 20);
    if (paid_pkh != witness.leaf.recipient_pkh) {
        err = "claim: payout pkh does not match leaf recipient";
        return false;
    }
    if (tx.vout[0].nValue != witness.leaf.amount) {
        err = "claim: payout amount does not match leaf amount";
        return false;
    }

    // 4. Replay protection.
    if (spent_set.IsSpent(leaf_hash)) {
        err = "claim: leaf already redeemed";
        return false;
    }

    // 5. Expiry.
    if (current_epoch > witness.leaf.epoch
        && current_epoch - witness.leaf.epoch > static_cast<uint32_t>(params.ClaimExpiryEpochs)) {
        err = "claim: expired";
        return false;
    }

    // Mark spent (caller is responsible for atomicity vs. block connection).
    spent_set.MarkSpent(leaf_hash);
    return true;
}

} // namespace dace
