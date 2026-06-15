// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <clientversion.h>
#include <crypto/scrypt.h>
#include <hash.h>
#include <tinyformat.h>

#include <cassert>
#include <cstring>

uint256 CBlockHeader::GetHash() const
{
    if (IsVericoin)
        return this->GetWorkHash();
    else
        return this->GetVeriumHash();
}

uint256 CBlockHeader::GetVeriumHash() const
{
    // SerializeHash respects dace::ExtendedSerializationScope via the
    // header's SerializationOp. When the scope is active, the hash covers
    // the extended 180-byte layout (DACE-1); otherwise the legacy 80-byte
    // layout. This makes the scope the single switch for "which header
    // hash am I computing?".
    return SerializeHash(*this);
}

namespace {

/** Build the canonical 80-byte scrypt² input buffer for a header.
 *
 *  Legacy mode (DACE not active):
 *      nVersion(4) || hashPrevBlock(32) || hashMerkleRoot(32) ||
 *      nTime(4)    || nBits(4)          || nNonce(4)
 *      = 80 bytes; identical to the existing memory layout under
 *      BEGIN(nVersion).
 *
 *  Extended mode (DACE active):
 *      same layout, but hashMerkleRoot is REPLACED by
 *      daceMerkleRoot = SHA256d( hashMerkleRoot || pairedAnchorRef ||
 *                                 beaconRef || rewardAccumulatorRoot ||
 *                                 LE32(epochIndex) )
 *      The PoS marker (high bit of nVersion) is intentionally included
 *      since DACE-1 folded the legacy nFlags PoS marker into nVersion.
 *
 *  This preserves the 80-byte scrypt² input contract — so every SIMD
 *  variant (avx2, avx512, shani, NEON, ...) continues to work without
 *  modification — while binding the entire extended header into the PoW
 *  input. See DACE-1 §"Rationale" and the PoWT mining philosophy doc.
 */
void BuildScryptInput(const CBlockHeader& h, unsigned char buf[80])
{
    unsigned char* p = buf;

    // nVersion (LE32)
    uint32_t ver = static_cast<uint32_t>(h.nVersion);
    memcpy(p, &ver, 4); p += 4;

    // hashPrevBlock (32)
    memcpy(p, h.hashPrevBlock.begin(), 32); p += 32;

    // hashMerkleRoot, or DACE substitution
    if (dace_block_extended_serialization_enabled()) {
        // daceMerkleRoot = SHA256d(merkleRoot || pairedAnchorRef ||
        //                          beaconRef || rewardAccumulatorRoot ||
        //                          LE32(epochIndex))
        CHashWriter w(SER_GETHASH, 0);
        w.write(reinterpret_cast<const char*>(h.hashMerkleRoot.begin()), 32);
        w.write(reinterpret_cast<const char*>(h.pairedAnchorRef.begin()), 32);
        w.write(reinterpret_cast<const char*>(h.beaconRef.begin()), 32);
        w.write(reinterpret_cast<const char*>(h.rewardAccumulatorRoot.begin()), 32);
        uint32_t e = h.epochIndex;
        w.write(reinterpret_cast<const char*>(&e), 4);
        uint256 daceMerkleRoot = w.GetHash();
        memcpy(p, daceMerkleRoot.begin(), 32);
    } else {
        memcpy(p, h.hashMerkleRoot.begin(), 32);
    }
    p += 32;

    // nTime, nBits, nNonce (each LE32)
    memcpy(p, &h.nTime,  4); p += 4;
    memcpy(p, &h.nBits,  4); p += 4;
    memcpy(p, &h.nNonce, 4); p += 4;

    assert(p - buf == 80);
}

} // namespace

uint256 CBlockHeader::GetWorkHash() const
{
    // scrypt² always consumes an 80-byte input. In legacy mode this is the
    // standard Bitcoin header layout. In DACE-1 extended mode, the merkle
    // root slot carries a commitment that binds all extended fields.
    //
    // Mining-surface impact (DACE-0 / Section 2.4.2): the only added work
    // per hash is one extra SHA256d over ~132 bytes, negligible relative
    // to scrypt²'s memory-hard N=1024² inner loop. CPU-fairness preserved.
    unsigned char input[80];
    BuildScryptInput(*this, input);
    uint256 thash;
    scryptHash(input, BEGIN(thash));
    return thash;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    // Legacy fields (always populated).
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, nFlags=%08x, vtx=%u",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        nFlags, vtx.size());
    // DACE-1 extended fields (printed only when non-null to keep legacy logs unchanged).
    if (!pairedAnchorRef.IsNull() || !beaconRef.IsNull() ||
        !rewardAccumulatorRoot.IsNull() || epochIndex != 0) {
        s << strprintf(", pairedAnchorRef=%s, beaconRef=%s, rewardAccumulatorRoot=%s, epochIndex=%u",
            pairedAnchorRef.ToString(),
            beaconRef.ToString(),
            rewardAccumulatorRoot.ToString(),
            epochIndex);
    }
    s << ")\n";
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
