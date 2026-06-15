#!/usr/bin/env python3
"""Run every scenario in order, printing all output to stdout.

Each scenario is invoked as a subprocess so the import-side path setup in
_common.setup_path() resolves the same way as a standalone invocation. This
keeps each scenario self-contained and runnable on its own.
"""

import os
import subprocess
import sys


SCENARIOS = [
    "happy_path",
    "beacon_reorg",
    "sortition_splitting",
    "stale_coupling",
    "recovery_anchor",
    "claim_redemption",
    "equivocation_slash",
]


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    print("=" * 78)
    print("  Binary Chain v3 (DACE) — full scenario sweep")
    print("=" * 78)
    failed = []
    for s in SCENARIOS:
        script = os.path.join(here, f"{s}.py")
        r = subprocess.run([sys.executable, script], capture_output=False)
        if r.returncode != 0:
            failed.append(s)
    print()
    print("=" * 78)
    if failed:
        print(f"  FAILED scenarios: {failed}")
        return 1
    print("  All scenarios complete.")
    print("=" * 78)
    return 0


if __name__ == "__main__":
    sys.exit(main())
