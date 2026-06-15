// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_NET_MESSAGES_H
#define BITCOIN_DACE_NET_MESSAGES_H

#include <dace/beacon.h>
#include <dace/joint_anchor.h>
#include <primitives/block.h>
#include <serialize.h>
#include <stdint.h>

namespace dace {

/** Chain identifier prefixed to cross-chain P2P messages. */
enum class ChainTag : uint8_t {
    Vericoin = 0x76, // 'v'
    Verium   = 0x77, // 'w'
};

/** getxheaders / xheaders payload: request and response for paired-chain
 *  header relay. See DACE-6. */
struct GetXHeaders {
    uint8_t chain_id{0};
    std::vector<uint256> block_locator;
    uint256 hash_stop;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(chain_id);
        READWRITE(block_locator);
        READWRITE(hash_stop);
    }
};

struct XHeaders {
    uint8_t chain_id{0};
    std::vector<CBlockHeader> headers;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(chain_id);
        READWRITE(headers);
    }
};

/** getja / ja: request and response for a Joint Anchor. */
struct GetJA {
    uint256 ja_hash;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ja_hash);
    }
};

struct JAMessage {
    JointAnchor anchor;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(anchor);
    }
};

/** jasig: single committee signature gossiped during the assembly window. */
struct JASig {
    uint32_t epoch_index{0};
    uint256 ja_partial_hash;
    uint16_t committee_index{0};
    std::vector<unsigned char> sig;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(epoch_index);
        READWRITE(ja_partial_hash);
        READWRITE(committee_index);
        READWRITE(sig);
    }
};

/** Wire command names. */
static constexpr const char* CMD_GETXHEADERS = "getxheaders";
static constexpr const char* CMD_XHEADERS    = "xheaders";
static constexpr const char* CMD_GETJA       = "getja";
static constexpr const char* CMD_JA          = "ja";
static constexpr const char* CMD_JASIG       = "jasig";

} // namespace dace

#endif // BITCOIN_DACE_NET_MESSAGES_H
