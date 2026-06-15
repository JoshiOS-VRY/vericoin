#!/usr/bin/env bash
# Binarytest end-to-end acceptance script.
#
# Brings up both daemons via docker compose, advances both chains past DACE
# activation (height 50), and asserts that binarychain_status reports
# `activated: true` on each side.
#
# This is the one-command smoke test for the Binary Chain v3 (DACE)
# developer environment described in vericoin/doc/dace/binarytest-network.md.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
COMPOSE_FILE="$REPO_ROOT/vericoin/test/binarychain/docker/docker-compose.yml"

if [[ ! -f "$COMPOSE_FILE" ]]; then
    echo "compose file missing at $COMPOSE_FILE" >&2
    exit 1
fi

echo "==> docker compose up"
docker compose -f "$COMPOSE_FILE" up -d --build

echo "==> waiting for healthchecks"
for _ in $(seq 1 60); do
    vrc_ok=$(docker inspect --format='{{.State.Health.Status}}' bc-vericoind 2>/dev/null || echo starting)
    vrm_ok=$(docker inspect --format='{{.State.Health.Status}}' bc-veriumd 2>/dev/null || echo starting)
    if [[ "$vrc_ok" == "healthy" && "$vrm_ok" == "healthy" ]]; then
        break
    fi
    sleep 2
done

echo "==> advance both chains past activation"
TARGET_VRC=70 TARGET_VRM=70 STEP=5 \
    bash "$REPO_ROOT/vericoin/test/binarychain/scripts/advance-to-activation.sh"

echo "==> verify activation"
verify_activated() {
    local container=$1
    local cli=$2
    local chain_arg=$3
    local out
    out=$(docker exec "$container" "$cli" -binarytest "$chain_arg" binarychain_status)
    echo "$out"
    if ! echo "$out" | grep -q '"activated": true'; then
        echo "ACTIVATION FAILED on $container" >&2
        return 1
    fi
}

verify_activated bc-vericoind vericoin-cli -vericoin
verify_activated bc-veriumd  verium-cli   -verium

echo "==> binarytest-demo: PASS"
