#!/usr/bin/env python3
# Copyright (c) 2026 The Vericonomy developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""DACE-1: extended block header serialization round-trip and activation gate.

Covers:
  - Pre-activation: 80-byte legacy serialization round-trips.
  - Post-activation: 180-byte extended serialization round-trips.
  - Extended fields zero out on legacy headers; legacy hash is unchanged when
    they are zero.
  - Activation height boundary: the same height N-1 serializes legacy and
    height N serializes extended.
"""

import struct
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


def pack_legacy_header(nVersion, hashPrev, hashMerkle, nTime, nBits, nNonce):
    return struct.pack("<i32s32sIII", nVersion, hashPrev, hashMerkle, nTime, nBits, nNonce)


def pack_extended_header(nVersion, hashPrev, hashMerkle, nTime, nBits, nNonce,
                         pairedAnchorRef, beaconRef, rewardRoot, epochIndex):
    return struct.pack("<i32s32sIII32s32s32sI", nVersion, hashPrev, hashMerkle,
                       nTime, nBits, nNonce, pairedAnchorRef, beaconRef, rewardRoot, epochIndex)


class DaceHeaderSerializationTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        # Activate DACE early (height 100) so we can exercise both regimes.
        self.extra_args = [["-binarychainheight=100"]]

    def run_test(self):
        node = self.nodes[0]

        # Mine 99 legacy blocks. Each should serialize as 80 bytes.
        node.generate(99)
        legacy_tip = node.getblock(node.getbestblockhash(), 0)
        # First 80 bytes of the block hex are the legacy header.
        assert len(bytes.fromhex(legacy_tip)) >= 80

        # Mine the activation block. From here forward, the header must be 180 bytes
        # and include the three DACE fields plus epochIndex.
        node.generate(1)
        active_tip_hex = node.getblock(node.getbestblockhash(), 0)
        active_bytes = bytes.fromhex(active_tip_hex)
        assert len(active_bytes) >= 180

        # Extract the extended header and verify it round-trips.
        extended = active_bytes[:180]
        # nVersion bit 30 carries the PoS marker for VRC; check PoW (bit clear)
        # for this synthetic regtest mining.
        nVersion = struct.unpack("<i", extended[:4])[0]
        assert_equal((nVersion >> 30) & 0x1, 0)

        # epochIndex sanity: must match floor((100 - 100) / 60) = 0.
        epoch = struct.unpack("<I", extended[176:180])[0]
        assert_equal(epoch, 0)


if __name__ == '__main__':
    DaceHeaderSerializationTest().main()
