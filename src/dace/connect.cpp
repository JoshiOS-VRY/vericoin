// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// DACE ConnectBlock / DisconnectBlock orchestration. Called from
// validation.cpp at the validation layer's existing hooks. Pre-activation
// (block height < BinaryChainHeight*) every entry point is a no-op so legacy
// chain behavior is bit-identical.

#include <dace/connect.h>

#include <chain.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <dace/activation.h>
#include <dace/anchor_lifecycle.h>
#include <dace/beacon.h>
#include <dace/joint_anchor.h>
#include <dace/metrics.h>
#include <dace/reward_accumulator.h>
#include <dace/service.h>
#include <logging.h>
#include <primitives/block.h>
#include <script/script.h>
#include <tinyformat.h>

namespace dace {

namespace {

// Track VRM coinbase JA inclusions across the local chain so the lifecycle
// state machine can decide when a certified anchor becomes activated. Keyed
// by full JA hash, value is the local height at which it was first observed
// in a VRM coinbase commitment.
//
// Held in process memory; rebuilt during IBD by replaying coinbases. A
// future revision flushes this to LevelDB alongside the ticket registry.
static std::map<uint256, int>& MutableVrmInclusions()
{
    static std::map<uint256, int> g_inclusions;
    return g_inclusions;
}

// Scan a block's coinbase for OP_RETURN payloads of the form
// <push 32 bytes JA hash>. Records each such hash as a VRM inclusion at the
// block's height. Anchors not yet observed are stored anyway so promotion
// can pick them up when their `ja` gossip arrives later.
void RecordVrmCoinbaseInclusions(const CBlock& block, int height)
{
    if (block.vtx.empty()) return;
    const CTransaction& cb = *block.vtx[0];
    for (const CTxOut& out : cb.vout) {
        const CScript& s = out.scriptPubKey;
        if (s.size() != 34) continue;
        if (s[0] != OP_RETURN || s[1] != 32) continue;
        uint256 ja_hash;
        std::memcpy(ja_hash.begin(), s.data() + 2, 32);
        MutableVrmInclusions()[ja_hash] = height;
    }
}

// Symmetric undo when a VRM block disconnects.
void EraseVrmCoinbaseInclusions(const CBlock& block, int height)
{
    if (block.vtx.empty()) return;
    const CTransaction& cb = *block.vtx[0];
    for (const CTxOut& out : cb.vout) {
        const CScript& s = out.scriptPubKey;
        if (s.size() != 34) continue;
        if (s[0] != OP_RETURN || s[1] != 32) continue;
        uint256 ja_hash;
        std::memcpy(ja_hash.begin(), s.data() + 2, 32);
        auto it = MutableVrmInclusions().find(ja_hash);
        if (it != MutableVrmInclusions().end() && it->second == height) {
            MutableVrmInclusions().erase(it);
        }
    }
}

bool IsEpochBoundary(int height, const Consensus::Params& params)
{
    if (params.fIsVericoin) {
        if (params.BinaryChainHeightVRC == Consensus::Params::DaceNotScheduled) return false;
        if (height < params.BinaryChainHeightVRC) return false;
        const int delta = height - params.BinaryChainHeightVRC;
        return delta > 0 && (delta % params.BeaconEpochVRC) == 0;
    }
    if (params.BinaryChainHeightVRM == Consensus::Params::DaceNotScheduled) return false;
    if (height < params.BinaryChainHeightVRM) return false;
    const int delta = height - params.BinaryChainHeightVRM;
    return delta > 0 && (delta % params.BeaconDelta) == 0;
}

} // anonymous namespace

static uint32_t EpochForChain(int height, const Consensus::Params& params)
{
    return params.fIsVericoin ? EpochIndexVRC(height, params)
                              : EpochIndexVRM(height, params);
}

bool CheckExtendedHeader(const CBlockHeader& header,
                         int height,
                         const Consensus::Params& params,
                         std::string& err)
{
    if (!IsActive(height, params)) {
        // Pre-activation: extended fields must be zero so the legacy
        // serialization & hash semantics match bit-for-bit. New miners
        // populate them only above activation height.
        if (!header.pairedAnchorRef.IsNull()
            || !header.beaconRef.IsNull()
            || !header.rewardAccumulatorRoot.IsNull()
            || header.epochIndex != 0) {
            err = "pre-activation block has non-zero DACE header fields";
            return false;
        }
        return true;
    }

    const uint32_t expected_epoch = EpochForChain(height, params);
    if (header.epochIndex != expected_epoch) {
        err = strprintf("epochIndex %u != expected %u at height %d",
                         header.epochIndex, expected_epoch, height);
        return false;
    }

    // pairedAnchorRef MUST reference the currently activated anchor on this
    // chain once one exists. Until the first JA activates, the field is
    // expected to be zero (genesis-of-coupling).
    if (Service::IsInitialized()) {
        const JointAnchor* active = Service::Get().Anchors().Active();
        if (active) {
            const uint256 active_hash = active->GetHash();
            if (header.pairedAnchorRef != active_hash) {
                err = "pairedAnchorRef does not match the currently activated JA";
                return false;
            }
        }
    }
    return true;
}

void OnConnectBlock(const CBlock& block,
                    const CBlockIndex* pindex,
                    const Consensus::Params& params)
{
    if (!pindex || !Service::IsInitialized()) return;
    const int height = pindex->nHeight;

    // Track VRM coinbase JA commitments regardless of activation; they may
    // appear pre-activation as observed-only metadata used to bootstrap the
    // lifecycle before the activation height passes.
    if (!params.fIsVericoin) {
        RecordVrmCoinbaseInclusions(block, height);
    }

    if (!IsActive(height, params)) return;

    Service& svc = Service::Get();
    const uint32_t epoch = EpochForChain(height, params);

    // ----- 1. Reward accumulator update --------------------------------------
    // Each block contributes the diverted portion of its subsidy (VRM) or
    // fees (VRC) into the chain-local accumulator. The accumulator's running
    // Merkle root is committed in the next epoch's Joint Anchor.
    if (!block.vtx.empty()) {
        const CTransaction& cb = *block.vtx[0];
        CAmount total = 0;
        for (const CTxOut& v : cb.vout) total += v.nValue;
        CAmount divert = params.fIsVericoin
                             ? DivertVrcFees(total, params)
                             : DivertVrmSubsidy(total, params);
        if (divert > 0) {
            ClaimLeaf leaf;
            leaf.source_chain = params.fIsVericoin
                                    ? ClaimSourceChain::VRC
                                    : ClaimSourceChain::VRM;
            leaf.source_block = block.GetHash();
            leaf.epoch = epoch;
            leaf.recipient_kind = params.fIsVericoin
                                      ? ClaimRecipientKind::VRM_BEACON_MINER
                                      : ClaimRecipientKind::VRC_TICKET_OPERATOR;
            // The recipient_pkh is filled in by the matching DACE-5 wallet
            // policy when the divert escrow output is created. The connect
            // path records the accumulator leaf either way so the per-epoch
            // root reflects the block's contribution.
            leaf.amount = divert;
            svc.CurrentAccumulator(epoch).AppendLeaf(leaf);
        }
    }

    // ----- 2. Epoch boundary closes the prev accumulator --------------------
    if (IsEpochBoundary(height, params) && epoch > 0) {
        const uint256 root = svc.CloseEpochAccumulator(epoch - 1);
        LogPrint(BCLog::VALIDATION,
                 "dace: closed epoch %u accumulator root=%s\n",
                 epoch - 1, root.ToString());
    }

    // ----- 3a. binarytest local committee auto-certify --------------------
    // On the isolated DACE test network the wallet developer needs the
    // chain to make Joint Anchor progress without running a real multi-node
    // committee ceremony. At each VRC epoch boundary, if the local
    // bonded-ticket pool meets the committee size, we synthesize and
    // observe a Certified+Activated JA so the protocol's downstream rules
    // (pairedAnchorRef in the next block, accumulator anchor, claim
    // witnesses) can be exercised end-to-end. This path is guarded to
    // binarytest only.
    {
        const std::string& net = Params().NetworkIDString();
        const bool is_binarytest = (net == CBaseChainParams::BINARYTEST_VERICOIN
                                 || net == CBaseChainParams::BINARYTEST_VERIUM);
        if (is_binarytest && params.fIsVericoin
            && IsEpochBoundary(height, params)
            && svc.Tickets().ActiveCountAt(epoch) >= params.CommitteeSize)
        {
            JointAnchor ja;
            ja.epoch_index = epoch;
            ja.vrc_checkpoint_hash = pindex->GetBlockHash();
            ja.vrc_checkpoint_height = height;
            ja.committee_root = svc.Tickets().RootAtEpoch(epoch);
            // Beacon is the last cached beacon for this epoch (if any); on
            // binarytest a missing beacon is fine because the local
            // simulator is not racing a real cross-chain consensus.
            Beacon b;
            if (svc.GetSelectedBeacon(epoch, b)) ja.beacon_ref = b.beacon_hash;
            ja.reward_root_vrc_prev = epoch > 0 ? svc.CurrentAccumulator(epoch - 1).CurrentRoot() : uint256();
            ja.reward_root_vrm_prev = uint256();
            // Stamp a committee signature placeholder so IsCertifiedShape()
            // is true; binarytest's local promoter does not enforce the
            // quorum signature verifier — see DACE-4 governance note.
            CommitteeSignature stub;
            stub.committee_index = 0;
            stub.sig = std::vector<unsigned char>{0xDA, 0xCE};
            ja.signature.signatures.push_back(stub);
            svc.Anchors().Observe(ja);
            // Record the JA as included in a paired-chain coinbase so the
            // lifecycle promoter accepts it as activated.
            MutableVrmInclusions()[ja.GetHash()] = height;
            LogPrint(BCLog::VALIDATION,
                     "dace[binarytest]: synthesized JA for epoch=%u at height=%d\n",
                     epoch, height);
        }
    }

    // ----- 3b. Anchor lifecycle: promote any certified -> activated --------
    const JointAnchor* promoted =
        svc.Anchors().PromoteIfActivated(height, MutableVrmInclusions(), params);
    if (promoted) {
        LogPrint(BCLog::VALIDATION,
                 "dace: activated JA epoch=%u hash=%s at vrc_height=%d\n",
                 promoted->epoch_index, promoted->GetHash().ToString(),
                 height);
    }

    // ----- 4. Clear stale-coupling if we made local progress ----------------
    if (svc.IsStaleCoupled() && (height % params.BeaconEpochVRC) == 0) {
        svc.SetStaleCoupled(false, "");
    }
}

void OnDisconnectBlock(const CBlock& block,
                       const CBlockIndex* pindex,
                       const Consensus::Params& params)
{
    if (!pindex || !Service::IsInitialized()) return;

    if (!params.fIsVericoin) {
        EraseVrmCoinbaseInclusions(block, pindex->nHeight);
    }

    if (!IsActive(pindex->nHeight, params)) return;

    Service& svc = Service::Get();
    svc.Anchors().Rewind(pindex->nHeight - 1, params);
    svc.Tickets().Rewind(EpochForChain(pindex->nHeight - 1, params));
}

} // namespace dace
