// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <dace/activation.h>
#include <dace/anchor_lifecycle.h>
#include <dace/claim.h>
#include <dace/joint_anchor.h>
#include <dace/metrics.h>
#include <dace/reward_accumulator.h>
#include <dace/service.h>
#include <dace/ticket_registry.h>
#include <key_io.h>
#include <miner.h>
#include <node/context.h>
#include <pow.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/script.h>
#include <shutdown.h>
#include <txmempool.h>
#include <util/system.h>
#include <validation.h>

#include <univalue.h>

#include <limits>

#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

namespace {

bool IsBinarytestNetwork()
{
    const std::string& id = Params().NetworkIDString();
    return id == CBaseChainParams::BINARYTEST_VERICOIN
        || id == CBaseChainParams::BINARYTEST_VERIUM;
}

/** Mine `n` PoW blocks to `coinbase_script`. Mirrors rpc/mining.cpp
 *  generateBlocks but kept local so dace.cpp does not depend on its static. */
UniValue MineBlocksTo(const CScript& coinbase_script, int n, uint64_t max_tries)
{
    UniValue hashes(UniValue::VARR);
    int height_end;
    int height;
    {
        LOCK(cs_main);
        height = ::ChainActive().Height();
        height_end = height + n;
    }
    unsigned int extra_nonce = 0;
    const CTxMemPool& mempool = EnsureMemPool();
    while (height < height_end && !ShutdownRequested()) {
        std::unique_ptr<CBlockTemplate> tmpl(
            BlockAssembler(mempool, Params()).CreateNewBlock(coinbase_script));
        if (!tmpl) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");
        }
        CBlock* pblock = &tmpl->block;
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock, ::ChainActive().Tip(), extra_nonce);
        }
        // Verium PoW validation checks GetWorkHash() (scrypt²), not GetHash().
        while (max_tries > 0
               && pblock->nNonce < std::numeric_limits<uint32_t>::max()
               && !CheckProofOfWork(pblock->GetWorkHash(), pblock->nBits, Params().GetConsensus())
               && !ShutdownRequested()) {
            ++pblock->nNonce;
            --max_tries;
        }
        if (max_tries == 0 || ShutdownRequested()) break;
        if (pblock->nNonce == std::numeric_limits<uint32_t>::max()) continue;
        std::shared_ptr<const CBlock> shared = std::make_shared<const CBlock>(*pblock);
        if (!ProcessNewBlock(Params(), shared, true, nullptr)) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock rejected block");
        }
        ++height;
        hashes.push_back(pblock->GetHash().GetHex());
    }
    return hashes;
}

} // anonymous namespace

static UniValue binarychain_status(const JSONRPCRequest& request)
{
    RPCHelpMan{"binarychain_status",
        "\nReturns current Binary Chain (DACE) status: active anchor, stale-coupling state,\n"
        "epoch, bonded ticket stats, paired-header lag percentiles.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "activated", "Whether DACE rules are in force locally"},
                {RPCResult::Type::NUM, "current_epoch", "Current coupling epoch"},
                {RPCResult::Type::STR, "active_anchor_hash", "Hash of the active Joint Anchor"},
                {RPCResult::Type::NUM, "active_anchor_epoch", "Epoch of the active Joint Anchor"},
                {RPCResult::Type::BOOL, "stale_coupled", "Whether paired-chain data is unavailable"},
                {RPCResult::Type::STR, "stale_reason", "Reason if stale_coupled"},
                {RPCResult::Type::NUM, "bonded_tickets_active", "Active ticket count"},
                {RPCResult::Type::NUM, "bonded_tickets_total_amount", "Total bonded amount"},
                {RPCResult::Type::NUM, "paired_header_lag_p50_sec", "P50 paired header lag"},
                {RPCResult::Type::NUM, "paired_header_lag_p95_sec", "P95 paired header lag"},
                {RPCResult::Type::NUM, "paired_header_lag_p99_sec", "P99 paired header lag"},
            }
        },
        RPCExamples{
            HelpExampleCli("binarychain_status", "")
            + HelpExampleRpc("binarychain_status", "")
        },
    }.Check(request);

    const Consensus::Params& params = Params().GetConsensus();
    LOCK(cs_main);
    UniValue obj(UniValue::VOBJ);

    const int tip_height = ::ChainActive().Height();
    const bool activated = dace::IsActive(tip_height, params);
    obj.pushKV("activated", activated);
    obj.pushKV("current_epoch", static_cast<uint64_t>(dace::EpochIndexVRC(tip_height, params)));

    if (dace::Service::IsInitialized()) {
        const dace::JointAnchor* a = dace::Service::Get().Anchors().Active();
        if (a) {
            obj.pushKV("active_anchor_hash", a->GetHash().ToString());
            obj.pushKV("active_anchor_epoch", static_cast<uint64_t>(a->epoch_index));
        } else {
            obj.pushKV("active_anchor_hash", UniValue());
            obj.pushKV("active_anchor_epoch", UniValue());
        }
        obj.pushKV("stale_coupled", dace::Service::Get().IsStaleCoupled());
        const uint32_t epoch = dace::EpochIndexVRC(tip_height, params);
        obj.pushKV("bonded_tickets_active",
                   static_cast<uint64_t>(dace::Service::Get().Tickets().ActiveCountAt(epoch)));
        obj.pushKV("bonded_tickets_total_amount",
                   ValueFromAmount(dace::Service::Get().Tickets().TotalBondedAt(epoch)));
    } else {
        obj.pushKV("active_anchor_hash", UniValue());
        obj.pushKV("active_anchor_epoch", UniValue());
        obj.pushKV("stale_coupled", false);
        obj.pushKV("bonded_tickets_active", 0);
        obj.pushKV("bonded_tickets_total_amount", ValueFromAmount(0));
    }

    double p50 = 0, p95 = 0, p99 = 0;
    dace::Metrics::Get().PairedHeaderLagPercentiles(p50, p95, p99);
    obj.pushKV("paired_header_lag_p50_sec", p50);
    obj.pushKV("paired_header_lag_p95_sec", p95);
    obj.pushKV("paired_header_lag_p99_sec", p99);

    return obj;
}

static UniValue binarychain_metrics(const JSONRPCRequest& request)
{
    RPCHelpMan{"binarychain_metrics",
        "\nReturns DACE threat-model counters. See design guide Section 13.1.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "divergent_beacon_selections", "Counter"},
                {RPCResult::Type::NUM, "reorgs_at_beacon", "Counter"},
                {RPCResult::Type::NUM, "committee_missed_votes", "Counter"},
                {RPCResult::Type::NUM, "foreign_payee_rejections", "Counter"},
                {RPCResult::Type::NUM, "consecutive_missed_anchors", "Counter"},
                {RPCResult::Type::NUM, "ibd_replay_consensus_diffs", "Counter"},
                {RPCResult::Type::NUM, "stale_coupling_entries", "Counter"},
                {RPCResult::Type::NUM, "recovery_anchors_built", "Counter"},
                {RPCResult::Type::NUM, "recovery_anchors_activated", "Counter"},
                {RPCResult::Type::NUM, "claim_redemptions_total", "Counter"},
                {RPCResult::Type::NUM, "claim_redemptions_rejected", "Counter"},
                {RPCResult::Type::NUM, "slashes_total", "Counter"},
                {RPCResult::Type::NUM, "top10_ticket_share", "Counter"},
                {RPCResult::Type::NUM, "seat_to_bonded_ratio_top10", "Counter"},
            }
        },
        RPCExamples{
            HelpExampleCli("binarychain_metrics", "")
            + HelpExampleRpc("binarychain_metrics", "")
        },
    }.Check(request);

    const dace::Metrics& m = dace::Metrics::Get();
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("divergent_beacon_selections", m.divergent_beacon_selections.load());
    obj.pushKV("reorgs_at_beacon", m.reorgs_at_beacon.load());
    obj.pushKV("committee_missed_votes", m.committee_missed_votes.load());
    obj.pushKV("foreign_payee_rejections", m.foreign_payee_rejections.load());
    obj.pushKV("consecutive_missed_anchors", m.consecutive_missed_anchors.load());
    obj.pushKV("ibd_replay_consensus_diffs", m.ibd_replay_consensus_diffs.load());
    obj.pushKV("stale_coupling_entries", m.stale_coupling_entries.load());
    obj.pushKV("recovery_anchors_built", m.recovery_anchors_built.load());
    obj.pushKV("recovery_anchors_activated", m.recovery_anchors_activated.load());
    obj.pushKV("claim_redemptions_total", m.claim_redemptions_total.load());
    obj.pushKV("claim_redemptions_rejected", m.claim_redemptions_rejected.load());
    obj.pushKV("slashes_total", m.slashes_total.load());
    obj.pushKV("top10_ticket_share", m.top10_ticket_share.load());
    obj.pushKV("seat_to_bonded_ratio_top10", m.seat_to_bonded_ratio_top10.load());
    return obj;
}

static UniValue binarychain_anchor(const JSONRPCRequest& request)
{
    RPCHelpMan{"binarychain_anchor",
        "\nReturns the currently activated Joint Anchor as a JSON object.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "epoch", "Joint Anchor epoch"},
                {RPCResult::Type::STR, "prev_anchor_hash", "Previous anchor hash"},
                {RPCResult::Type::STR, "beacon_ref", "Beacon reference hash"},
                {RPCResult::Type::STR, "vrc_checkpoint_hash", "VRC checkpoint hash"},
                {RPCResult::Type::NUM, "vrc_checkpoint_height", "VRC checkpoint height"},
                {RPCResult::Type::STR, "reward_root_vrc_prev", "Previous VRC reward root"},
                {RPCResult::Type::STR, "reward_root_vrm_prev", "Previous VRM reward root"},
                {RPCResult::Type::STR, "committee_root", "Committee root"},
            }
        },
        RPCExamples{
            HelpExampleCli("binarychain_anchor", "")
            + HelpExampleRpc("binarychain_anchor", "")
        },
    }.Check(request);

    if (!dace::Service::IsInitialized()) {
        return UniValue();
    }
    const dace::JointAnchor* a = dace::Service::Get().Anchors().Active();
    if (!a) return UniValue();

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("epoch", static_cast<uint64_t>(a->epoch_index));
    obj.pushKV("prev_anchor_hash", a->prev_anchor_hash.ToString());
    obj.pushKV("beacon_ref", a->beacon_ref.ToString());
    obj.pushKV("vrc_checkpoint_hash", a->vrc_checkpoint_hash.ToString());
    obj.pushKV("vrc_checkpoint_height", static_cast<uint64_t>(a->vrc_checkpoint_height));
    obj.pushKV("reward_root_vrc_prev", a->reward_root_vrc_prev.ToString());
    obj.pushKV("reward_root_vrm_prev", a->reward_root_vrm_prev.ToString());
    obj.pushKV("committee_root", a->committee_root.ToString());
    return obj;
}

// -----------------------------------------------------------------------------
// binarychain_fund_wallet — binarytest-only RPC that mines `nblocks` PoW blocks
// (with the loaded wallet's coinbase) so a fresh wallet on the isolated DACE
// test network has spendable / stakeable funds without an external faucet.
//
// Refuses to run on any non-binarytest network.
// -----------------------------------------------------------------------------
static UniValue binarychain_fund_wallet(const JSONRPCRequest& request)
{
    RPCHelpMan{"binarychain_fund_wallet",
        "\nBinarytest-only: mine `nblocks` PoW blocks to a newly-derived wallet address so the\n"
        "wallet has spendable VRM or stakeable VRC. Refused on any other network.\n",
        {
            {"nblocks", RPCArg::Type::NUM, /* default */ "10",
             "Number of blocks to mine. Defaults to 10 (well past nMaturity=4)."},
            {"address", RPCArg::Type::STR, /* default */ "",
             "Optional explicit destination. If omitted, a new wallet address is used."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "network", "Active network id"},
                {RPCResult::Type::STR, "address", "Funded address"},
                {RPCResult::Type::ARR, "blocks", "Mined block hashes",
                    {
                        {RPCResult::Type::STR, "", "Block hash"},
                    }},
            }
        },
        RPCExamples{
            HelpExampleCli("binarychain_fund_wallet", "10")
            + HelpExampleRpc("binarychain_fund_wallet", "10")
        },
    }.Check(request);

    if (!IsBinarytestNetwork()) {
        throw JSONRPCError(RPC_METHOD_NOT_FOUND,
            "binarychain_fund_wallet is only available on the binarytest network.");
    }

    int nblocks = request.params[0].isNull() ? 10 : request.params[0].get_int();
    if (nblocks <= 0 || nblocks > 1000) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "nblocks must be between 1 and 1000");
    }

    CTxDestination dest;
    if (!request.params[1].isNull() && !request.params[1].get_str().empty()) {
        dest = DecodeDestination(request.params[1].get_str());
        if (!IsValidDestination(dest)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
        }
    } else {
        std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
        CWallet* const pwallet = wallet.get();
        if (!pwallet) {
            throw JSONRPCError(RPC_WALLET_NOT_FOUND,
                "No wallet loaded; pass `address` or load a wallet first.");
        }
        LOCK(pwallet->cs_wallet);
        OutputType out_type = pwallet->m_default_address_type;
        std::string label;
        std::string error;
        if (!pwallet->GetNewDestination(out_type, label, dest, error)) {
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, error);
        }
    }

    UniValue hashes = MineBlocksTo(GetScriptForDestination(dest), nblocks, 1000000);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("network", Params().NetworkIDString());
    obj.pushKV("address", EncodeDestination(dest));
    obj.pushKV("blocks", hashes);
    return obj;
}

// -----------------------------------------------------------------------------
// DACE ticket / claim RPCs. Phase 1: register / unbond / redeem accept the
// caller's intent and stage state in dace::Service. Full consensus enforcement
// (slashing, in-coinbase commitments, lifecycle promotion) is wired in the
// validation layer in p2-connectblock.
// -----------------------------------------------------------------------------
static UniValue binarychain_register_ticket(const JSONRPCRequest& request)
{
    RPCHelpMan{"binarychain_register_ticket",
        "\nRegister a bonded ticket (VRC side). The stake outpoint must hold\n"
        "exactly TicketStakeUnit. The operator_pubkey signs committee duties.\n",
        {
            {"stake_outpoint", RPCArg::Type::STR, RPCArg::Optional::NO,
             "Outpoint formatted txid:vout."},
            {"operator_pubkey", RPCArg::Type::STR, RPCArg::Optional::NO,
             "33-byte compressed pubkey hex used to sign JA quorum messages."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "ticket_id", "Ticket id"},
                {RPCResult::Type::NUM, "registered_at_epoch", "Registration epoch"},
                {RPCResult::Type::NUM, "active_from_epoch", "Activation epoch"},
            }
        },
        RPCExamples{HelpExampleCli("binarychain_register_ticket",
                                    "\"abc...:0\" \"02ab...\"")},
    }.Check(request);

    if (!dace::Service::IsInitialized()) {
        throw JSONRPCError(RPC_IN_WARMUP, "DACE service not initialized");
    }

    const std::string outpoint_str = request.params[0].get_str();
    const std::string pubkey_str = request.params[1].get_str();
    auto colon = outpoint_str.find(':');
    if (colon == std::string::npos) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "stake_outpoint must be txid:vout");
    }
    uint256 prev_txid = uint256S(outpoint_str.substr(0, colon));
    uint32_t prev_n = static_cast<uint32_t>(std::stoul(outpoint_str.substr(colon + 1)));

    std::vector<unsigned char> pubkey_bytes = ParseHex(pubkey_str);
    CPubKey operator_pubkey(pubkey_bytes.begin(), pubkey_bytes.end());
    if (!operator_pubkey.IsFullyValid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "operator_pubkey is not a valid compressed pubkey");
    }

    const uint32_t epoch = dace::EpochIndexVRC(::ChainActive().Height(), Params().GetConsensus());
    dace::Ticket t;
    t.stake_outpoint = COutPoint(prev_txid, prev_n);
    t.operator_pubkey = operator_pubkey;
    t.registered_epoch = epoch + 1;
    t.unbond_epoch = 0;
    t.stake_amount = Params().GetConsensus().TicketStakeUnit;

    std::string err;
    if (!dace::Service::Get().Tickets().InsertDirect(t, err)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    }

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("ticket_id", t.GetId().ToString());
    obj.pushKV("registered_at_epoch", static_cast<uint64_t>(epoch));
    obj.pushKV("active_from_epoch", static_cast<uint64_t>(t.registered_epoch));
    return obj;
}

static UniValue binarychain_unbond_ticket(const JSONRPCRequest& request)
{
    RPCHelpMan{"binarychain_unbond_ticket",
        "\nInitiate unbond of an active ticket. Becomes spendable after\n"
        "TicketUnbondDelayEpochs.\n",
        {{"ticket_id", RPCArg::Type::STR, RPCArg::Optional::NO, "32-byte ticket id hex."}},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "ticket_id", "Ticket id"},
                {RPCResult::Type::NUM, "unbond_at_epoch", "Unbond epoch"},
            }
        },
        RPCExamples{HelpExampleCli("binarychain_unbond_ticket", "\"abc...\"")},
    }.Check(request);

    if (!dace::Service::IsInitialized()) {
        throw JSONRPCError(RPC_IN_WARMUP, "DACE service not initialized");
    }
    uint256 tid = uint256S(request.params[0].get_str());
    const uint32_t epoch = dace::EpochIndexVRC(::ChainActive().Height(), Params().GetConsensus());
    const uint32_t unbond_at = epoch + Params().GetConsensus().TicketUnbondDelayEpochs;
    std::string err;
    if (!dace::Service::Get().Tickets().BeginUnbond(tid, unbond_at, err)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, err);
    }
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("ticket_id", tid.ToString());
    obj.pushKV("unbond_at_epoch", static_cast<uint64_t>(unbond_at));
    return obj;
}

static UniValue binarychain_redeem_claim(const JSONRPCRequest& request)
{
    RPCHelpMan{"binarychain_redeem_claim",
        "\nRedeem a DACE cross-chain reward claim for the given leaf hash.\n"
        "Returns the txid of the broadcast claim-redemption transaction once\n"
        "the validation layer wires the witness construction (phase p2).\n",
        {{"leaf_hash", RPCArg::Type::STR, RPCArg::Optional::NO,
          "32-byte claim leaf hash from the reward accumulator."}},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "txid", "Broadcast txid"},
                {RPCResult::Type::STR, "status", "Redemption status"},
            }
        },
        RPCExamples{HelpExampleCli("binarychain_redeem_claim", "\"abc...\"")},
    }.Check(request);

    if (!dace::Service::IsInitialized()) {
        throw JSONRPCError(RPC_IN_WARMUP, "DACE service not initialized");
    }
    uint256 leaf = uint256S(request.params[0].get_str());
    UniValue obj(UniValue::VOBJ);
    if (dace::Service::Get().Claims().IsSpent(leaf)) {
        obj.pushKV("txid", UniValue());
        obj.pushKV("status", "already_redeemed");
        return obj;
    }
    // Reserve the claim. Full witness assembly + broadcast lands with the
    // validation-layer wiring in p2-connectblock and p2-dace-rpc.
    dace::Service::Get().Claims().MarkSpent(leaf);
    obj.pushKV("txid", UniValue());
    obj.pushKV("status", "reserved");
    return obj;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category               name                          actor (function)               argNames
  //  --------------------- ---------------------------   ----------------------------   ----------
    { "dace",               "binarychain_status",         &binarychain_status,           {} },
    { "dace",               "binarychain_metrics",        &binarychain_metrics,          {} },
    { "dace",               "binarychain_anchor",         &binarychain_anchor,           {} },
    { "dace",               "binarychain_fund_wallet",    &binarychain_fund_wallet,      {"nblocks","address"} },
    { "dace",               "binarychain_register_ticket",&binarychain_register_ticket,  {"stake_outpoint","operator_pubkey"} },
    { "dace",               "binarychain_unbond_ticket",  &binarychain_unbond_ticket,    {"ticket_id"} },
    { "dace",               "binarychain_redeem_claim",   &binarychain_redeem_claim,     {"leaf_hash"} },
};
// clang-format on

void RegisterDaceRPCCommands(CRPCTable& t)
{
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
