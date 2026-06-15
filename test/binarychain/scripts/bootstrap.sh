#!/usr/bin/env bash
# Binary Chain v3 (DACE) — bootstrap a binarytest network from scratch.
#
# Brings up vericoind + veriumd in docker-compose, waits for both to be
# healthy, and reports their initial status.
#
# Idempotent: safe to re-run; uses --remove-orphans for cleanliness.

set -euo pipefail

cd "$(dirname "$0")/../docker"

echo "==> Building images"
docker compose build

echo "==> Starting vericoind + veriumd + observer (binarytest mode)"
docker compose up -d --remove-orphans

echo "==> Waiting for daemons to be healthy..."
deadline=$(( $(date +%s) + 180 ))
while :; do
    vrc=$(docker inspect -f '{{.State.Health.Status}}' bc-vericoind 2>/dev/null || echo "starting")
    vrm=$(docker inspect -f '{{.State.Health.Status}}' bc-veriumd 2>/dev/null || echo "starting")
    if [[ "$vrc" == "healthy" && "$vrm" == "healthy" ]]; then
        break
    fi
    if (( $(date +%s) > deadline )); then
        echo "ERROR: daemons not healthy after 180s. Check 'docker compose logs'."
        exit 1
    fi
    printf "    vrc=%s  vrm=%s\n" "$vrc" "$vrm"
    sleep 5
done

echo "==> Both daemons healthy. Initial status:"
echo "--- VRC binarychain_status ---"
docker exec bc-vericoind vericoin-cli -binarytest -vericoin binarychain_status || true
echo "--- VRM binarychain_status ---"
docker exec bc-veriumd verium-cli -binarytest -verium binarychain_status || true

echo
echo "==> Observer metrics at http://localhost:9101/metrics"
echo "==> Next: run scripts/advance-to-activation.sh to push both chains past DACE activation."
