// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <clientversion.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
/**
 * Block header.
 *
 * Legacy layout (pre-DACE activation):
 *   nVersion | hashPrevBlock | hashMerkleRoot | nTime | nBits | nNonce
 *   plus an out-of-hash nFlags ppcoin marker preserved via SER_POSMARKER.
 *
 * Extended layout (DACE active, per DACE-1):
 *   nVersion | hashPrevBlock | hashMerkleRoot | nTime | nBits | nNonce |
 *   pairedAnchorRef | beaconRef | rewardAccumulatorRoot | epochIndex
 *
 * In the extended layout the PoS marker moves into nVersion bit 30 so the
 * marker is included in the header hash. The legacy nFlags field is still
 * stored on the in-memory CBlockHeader (kept for backwards compatibility with
 * deserialised legacy blocks) but is not serialised once DACE is active.
 *
 * The serialization path is selected via a thread-local "extended mode" flag
 * set by callers that know the block's height vs. the activation height.
 * See dace::WithExtendedSerialization.
 */
/** Thread-local accessor for DACE-1 extended-header serialization mode.
 *  Defined in dace/header.cpp. Returns true when the current scope is
 *  inside dace::ExtendedSerializationScope. */
bool dace_block_extended_serialization_enabled();

class CBlockHeader
{
public:
    // legacy fields
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    // ppcoin legacy marker (still populated for legacy blocks, no longer
    // serialized after DACE activation)
    uint32_t nFlags;

    // DACE-1 extended fields (populated only for blocks at or above activation)
    uint256 pairedAnchorRef;
    uint256 beaconRef;
    uint256 rewardAccumulatorRoot;
    uint32_t epochIndex;

    static const int32_t CURRENT_VERSION = 7;
    static const int32_t NORMAL_SERIALIZE_SIZE = 80;
    static const int32_t EXTENDED_SERIALIZE_SIZE = 180;

    /** PoS marker is bit 30 of nVersion in the extended layout. */
    static const int32_t POS_VERSION_BIT_MASK = (1 << 30);

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);

        if (UseExtendedSerialization(s)) {
            // DACE-1 extended header. Always part of the hash; no SER_POSMARKER
            // gating because the PoS marker now lives in nVersion bit 30.
            READWRITE(pairedAnchorRef);
            READWRITE(beaconRef);
            READWRITE(rewardAccumulatorRoot);
            READWRITE(epochIndex);
        } else {
            // Legacy ppcoin: do not serialize nFlags when computing hash.
            if (!(s.GetType() & SER_GETHASH) && s.GetType() & SER_POSMARKER)
                READWRITE(nFlags);
        }
    }

    void SetNull()
    {
        nVersion = CURRENT_VERSION;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        nFlags = 0;
        pairedAnchorRef.SetNull();
        beaconRef.SetNull();
        rewardAccumulatorRoot.SetNull();
        epochIndex = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    /** PoS-marker helpers for the DACE-1 extended layout. Independent of nFlags. */
    bool IsProofOfStakeMarked() const { return (nVersion & POS_VERSION_BIT_MASK) != 0; }
    void SetProofOfStakeMarked(bool fSet)
    {
        if (fSet) nVersion |= POS_VERSION_BIT_MASK;
        else nVersion &= ~POS_VERSION_BIT_MASK;
    }

    uint256 GetHash() const;
    uint256 GetVeriumHash() const;
    uint256 GetWorkHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

private:
    /** Decide whether to use DACE-1 extended serialization for this stream.
     *  Currently consults a thread-local flag set by
     *  dace::ExtendedSerializationScope (see dace/header.h). The Stream
     *  argument is reserved for future use (e.g., distinguishing wire vs
     *  disk encoding). */
    template <typename Stream>
    static bool UseExtendedSerialization(Stream& s) {
        (void)s;
        return ::dace_block_extended_serialization_enabled();
    }
};

class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // ppcoin: block signature - signed by coin base txout[0]'s owner
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CBlockHeader, *this);
        READWRITE(vtx);

        if( IsVericoin )
            READWRITE(vchBlockSig);

    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
        vchBlockSig.clear();
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion              = nVersion;
        block.hashPrevBlock         = hashPrevBlock;
        block.hashMerkleRoot        = hashMerkleRoot;
        block.nTime                 = nTime;
        block.nBits                 = nBits;
        block.nNonce                = nNonce;
        block.nFlags                = nFlags;
        // DACE-1 extended fields (zero/null on legacy blocks).
        block.pairedAnchorRef       = pairedAnchorRef;
        block.beaconRef             = beaconRef;
        block.rewardAccumulatorRoot = rewardAccumulatorRoot;
        block.epochIndex            = epochIndex;
        return block;
    }

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    bool IsProofOfStake() const
    {
        if( !IsVericoin ) {
            return false;
        }
        return (vtx.size() > 1 && vtx[1]->IsCoinStake());
    }

    bool IsProofOfWork() const
    {
        return !IsProofOfStake();
    }

    std::pair<COutPoint, unsigned int> GetProofOfStake() const
    {
        return IsProofOfStake() ? std::make_pair(vtx[1]->vin[0].prevout, vtx[1]->nTime) : std::make_pair(COutPoint(), (unsigned int)0);
    }

    // ppcoin: get max transaction timestamp
    int64_t GetMaxTransactionTime() const
    {
        int64_t maxTransactionTime = 0;
        for (const auto& tx : vtx)
            maxTransactionTime = std::max(maxTransactionTime, (int64_t)tx->nTime);
        return maxTransactionTime;
    }

    unsigned int GetStakeEntropyBit() const; // ppcoin: entropy bit for stake modifier if chosen by modifier

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H
