// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparamsbase.h>

#include <tinyformat.h>
#include <util/system.h>
#include <util/memory.h>

#include <assert.h>

const std::string CBaseChainParams::VERICOIN = "vericoin";
const std::string CBaseChainParams::VERIUM = "verium";
const std::string CBaseChainParams::BINARYTEST_VERICOIN = "binarytest-vericoin";
const std::string CBaseChainParams::BINARYTEST_VERIUM   = "binarytest-verium";

void SetupChainParamsBaseOptions()
{
    gArgs.AddArg("-chain=<chain>",
        "Use the chain <chain> (default: vericoin). Allowed values: "
        "vericoin, verium, binarytest-vericoin, binarytest-verium",
        ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-vericoin", "Use Vericoin chain. Equivalent to -chain=vericoin.", ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-verium", "Use Verium chain. Equivalent to -chain=verium.", ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-binarytest",
        "Isolated Binary Chain v3 (DACE) test network. Combine with "
        "-vericoin or -verium to select the chain side. Cannot connect to "
        "mainnet peers; uses distinct ports (41683/41684 VRC, 41987/41988 VRM) "
        "and message-start magic 0x44 0x41 0x43 0x45 (\"DACE\").",
        ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
}

static std::unique_ptr<CBaseChainParams> globalChainBaseParams;

const CBaseChainParams& BaseParams()
{
    assert(globalChainBaseParams);
    return *globalChainBaseParams;
}

std::unique_ptr<CBaseChainParams> CreateBaseChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::VERICOIN)
        return MakeUnique<CBaseChainParams>("vericoin", 58683);
    else if (chain == CBaseChainParams::VERIUM)
        return MakeUnique<CBaseChainParams>("verium", 33987);
    else if (chain == CBaseChainParams::BINARYTEST_VERICOIN)
        return MakeUnique<CBaseChainParams>("binarytest-vericoin", 41683);
    else if (chain == CBaseChainParams::BINARYTEST_VERIUM)
        return MakeUnique<CBaseChainParams>("binarytest-verium", 41987);
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectBaseParams(const std::string& chain)
{
    globalChainBaseParams = CreateBaseChainParams(chain);
    gArgs.SelectConfigNetwork(chain);
}
