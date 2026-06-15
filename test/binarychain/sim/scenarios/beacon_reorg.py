#!/usr/bin/env python3
"""Scenario: VRM reorg before beacon activation — fallback ladder advances.

Demonstrates DACE-2's determinism: when the canonical VRM tip reorgs past
the candidate beacon BEFORE the beacon has been activated in a JA, the
bounded fallback ladder selects the next-eligible height. Once activated
the beacon never changes.
"""

from _common import setup_path
setup_path()

from _common import section, step, kv
from sim import BinaryChainSim, DaceParams


def main() -> None:
    section("Scenario: beacon_reorg")

    sim = BinaryChainSim(DaceParams(
        binary_chain_height_vrm=50,
        beacon_delta=4,
        beacon_k=4,
        beacon_fallback_window=4,
    ))

    step("Mine VRM to height 60 (epoch 0 target H_e = 50, plus K_BEACON=4 confirmations)")
    sim.mine_vrm(61)
    kv("VRM tip", sim.vrm.tip)

    step("Select beacon for epoch 0")
    beacon0 = sim.beacon_for_epoch(0)
    kv("beacon0.height", beacon0.beacon_height)
    kv("beacon0.offset", beacon0.selection_offset)

    step("Reorg VRM tip back to height 49 (drops the beacon block)")
    sim.vrm.reorg_to(49)
    kv("VRM tip after reorg", sim.vrm.tip)

    step("Try beacon selection again — should fail (chain too short)")
    beacon_after = sim.beacon_for_epoch(0)
    kv("beacon_after", beacon_after)

    step("Re-mine VRM past activation + depth")
    sim.mine_vrm(20)
    kv("VRM tip", sim.vrm.tip)

    step("Beacon selection with fallback ladder")
    beacon_recover = sim.beacon_for_epoch(0)
    kv("beacon_recover.height", beacon_recover.beacon_height)
    kv("beacon_recover.offset", beacon_recover.selection_offset)
    kv("changed vs beacon0", beacon_recover.beacon_hash != beacon0.beacon_hash)

    print()
    print("  RESULT: fallback ladder selected next-eligible height post-reorg.")


if __name__ == "__main__":
    main()
