#!/usr/bin/env bash
# Tear down the binarytest network and wipe all state.

set -euo pipefail
cd "$(dirname "$0")/../docker"

docker compose down -v --remove-orphans
echo "==> binarytest network torn down. All volumes wiped."
