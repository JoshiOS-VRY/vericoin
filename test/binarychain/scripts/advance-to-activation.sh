#!/usr/bin/env bash
# Advance both binarytest chains past DACE activation (height 50 by default).
#
# VRM is a PoW chain: we drive it with binarychain_fund_wallet which mines
# blocks into the loaded wallet.
# VRC is a PoST chain: generatetoaddress does not produce valid PoST blocks,
# so we use binarychain_fund_wallet there too (the binarytest VRC chain
# accepts PoW blocks for the first nMaturity blocks so the test environment
# can bootstrap funds + stakes).
#
# Both calls are idempotent: re-running the script after activation is a
# no-op once the target heights are reached.

set -euo pipefail

TARGET_VRC=${TARGET_VRC:-70}
TARGET_VRM=${TARGET_VRM:-70}
STEP=${STEP:-5}

vrc_h() { docker exec bc-vericoind vericoin-cli -binarytest -vericoin getblockcount; }
vrm_h() { docker exec bc-veriumd  verium-cli -binarytest -verium getblockcount; }

advance_vrm() {
  while [[ $(vrm_h) -lt "$TARGET_VRM" ]]; do
    docker exec bc-veriumd verium-cli -binarytest -verium \
        binarychain_fund_wallet "$STEP" > /dev/null
    printf "    VRM=%s\n" "$(vrm_h)"
    sleep 1
  done
}

advance_vrc() {
  while [[ $(vrc_h) -lt "$TARGET_VRC" ]]; do
    docker exec bc-vericoind vericoin-cli -binarytest -vericoin \
        binarychain_fund_wallet "$STEP" > /dev/null
    printf "    VRC=%s\n" "$(vrc_h)"
    sleep 1
  done
}

echo "==> Generating VRM blocks up to height $TARGET_VRM"
advance_vrm

echo "==> Generating VRC blocks up to height $TARGET_VRC"
advance_vrc

echo "==> Both chains past activation. binarychain_status:"
echo "--- VRC ---"
docker exec bc-vericoind vericoin-cli -binarytest -vericoin binarychain_status
echo "--- VRM ---"
docker exec bc-veriumd verium-cli -binarytest -verium binarychain_status
