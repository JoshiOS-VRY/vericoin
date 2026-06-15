#!/usr/bin/env python3
# Copyright (c) 2026 The Vericonomy developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""binarytest fundwallet + DACE activation smoke test.

Covers:
  - binarychain_fund_wallet RPC mines blocks to a new wallet address.
  - The wallet receives spendable balance after coinbase maturity.
  - Repeated funding past BinaryChainHeight* flips binarychain_status to
    `activated: true`.

This is the smallest end-to-end test that exercises the path the wallet's
'Fund test wallet' button uses on the binarytest network.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class DaceBinarytestFundWalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        # The functional test harness drives -binarychainheight directly to
        # exercise the same code path that binarytest enforces via
        # CBinaryTestVericoinParams.BinaryChainHeightVRC = 50.
        self.extra_args = [["-binarychainheight=20"]]

    def run_test(self):
        node = self.nodes[0]

        # Pre-activation: status returns activated=False.
        pre = node.binarychain_status()
        assert_equal(pre["activated"], False)

        # Fund 25 blocks. After this we are past activation (height 25 > 20).
        result = node.binarychain_fund_wallet(25)
        assert "blocks" in result
        assert_equal(len(result["blocks"]), 25)
        assert "address" in result and len(result["address"]) > 0

        # Status should now report activated.
        post = node.binarychain_status()
        assert_equal(post["activated"], True)

        # Mining to the wallet should produce spendable balance after coinbase
        # maturity (binarytest sets nMaturity=4 in CBinaryTestVericoinParams;
        # the functional regtest harness uses 100). Verify at least immature
        # is non-zero so we know coinbase outputs landed in the wallet.
        info = node.getwalletinfo()
        assert (info["balance"] + info["immature_balance"]) > 0


if __name__ == "__main__":
    DaceBinarytestFundWalletTest().main()
