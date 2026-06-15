#!/usr/bin/env bash
# Pretty-print the binarychain_status + binarychain_metrics on both daemons.

set -euo pipefail

echo "=========================================================================="
echo "  VRC binarychain_status"
echo "=========================================================================="
docker exec bc-vericoind vericoin-cli -binarytest -vericoin binarychain_status | jq .

echo
echo "=========================================================================="
echo "  VRC binarychain_metrics"
echo "=========================================================================="
docker exec bc-vericoind vericoin-cli -binarytest -vericoin binarychain_metrics | jq .

echo
echo "=========================================================================="
echo "  VRM binarychain_status"
echo "=========================================================================="
docker exec bc-veriumd verium-cli -binarytest -verium binarychain_status | jq .

echo
echo "=========================================================================="
echo "  VRM binarychain_metrics"
echo "=========================================================================="
docker exec bc-veriumd verium-cli -binarytest -verium binarychain_metrics | jq .
