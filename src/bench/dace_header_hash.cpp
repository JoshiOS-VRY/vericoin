// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <dace/header.h>
#include <primitives/block.h>

/**
 * CPU-fairness preservation benchmark (DACE-0 Section 2.4.2).
 *
 * Measures scrypt² hash throughput on a block header in two modes:
 *   1. Legacy 80-byte serialization.
 *   2. DACE-1 extended layout (scrypt² still consumes 80 bytes — the merkle
 *      root slot carries the daceMerkleRoot substitution).
 *
 * Expected: throughput difference between modes is bounded by the cost of one
 * extra SHA256d over ~132 bytes per hash attempt. Relative to N=1024² scrypt²
 * memory-hard work, this is negligible (single-digit microseconds per ~100ms
 * hash attempt at typical mainnet difficulty).
 *
 * Required Phase 1 gate: relative slowdown < 1.05x (5%).
 */

namespace {

CBlockHeader MakeSampleHeader()
{
    CBlockHeader h;
    h.nVersion = 1 | (1 << 30); // mark PoS bit so we exercise the layout
    h.nTime = 1700000000;
    h.nBits = 0x1d00ffff;
    h.nNonce = 0;
    // Populate extended fields so daceMerkleRoot substitution path is non-trivial.
    h.pairedAnchorRef.SetNull();
    h.beaconRef.SetNull();
    h.rewardAccumulatorRoot.SetNull();
    h.epochIndex = 42;
    return h;
}

} // namespace

static void DaceHeaderHashLegacy(benchmark::State& state)
{
    CBlockHeader h = MakeSampleHeader();
    while (state.KeepRunning()) {
        // Legacy mode: no scope, GetWorkHash builds the standard 80-byte
        // input.
        (void)h.GetWorkHash();
    }
}

BENCHMARK(DaceHeaderHashLegacy, 100);

static void DaceHeaderHashExtended(benchmark::State& state)
{
    CBlockHeader h = MakeSampleHeader();
    // Populate extended fields with non-null values so the merkle-substitution
    // SHA256d path covers realistic content.
    for (size_t i = 0; i < 32; ++i) h.pairedAnchorRef.begin()[i]       = static_cast<unsigned char>(0x10 + i);
    for (size_t i = 0; i < 32; ++i) h.beaconRef.begin()[i]             = static_cast<unsigned char>(0x20 + i);
    for (size_t i = 0; i < 32; ++i) h.rewardAccumulatorRoot.begin()[i] = static_cast<unsigned char>(0x30 + i);
    while (state.KeepRunning()) {
        dace::ExtendedSerializationScope scope;
        (void)h.GetWorkHash();
    }
}

BENCHMARK(DaceHeaderHashExtended, 100);
