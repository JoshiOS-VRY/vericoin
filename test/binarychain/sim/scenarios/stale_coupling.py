#!/usr/bin/env python3
"""Scenario: stale-coupling mode under paired-chain data unavailability.

Demonstrates DACE-7: when VRC cannot see fresh VRM headers needed to compute
the next beacon, the local chain enters stale-coupling. Local block
processing continues; only finality upgrades pause. When VRM data returns,
the chain resumes finality.
"""

from _common import setup_path
setup_path()

from _common import section, step, kv
from sim import BinaryChainSim, DaceParams


def main() -> None:
    section("Scenario: stale_coupling")

    params = DaceParams(
        binary_chain_height_vrc=50,
        binary_chain_height_vrm=50,
        beacon_delta=4,
        beacon_k=4,
        beacon_epoch_vrc=10,
        stale_grace_epochs=2,
        stale_max_epochs=6,
    )
    sim = BinaryChainSim(params)

    step("Both chains advance through activation; one epoch passes cleanly")
    sim.mine_vrm(60)
    sim.stake_vrc(60)
    kv("VRM tip", sim.vrm.tip)
    kv("VRC tip", sim.vrc.tip)
    kv("epoch", sim.current_epoch_vrc())

    step("Simulate paired-data outage: VRM chain stops advancing")
    sim.stake_vrc(50)  # ~5 more epochs of VRC alone
    kv("VRM tip", sim.vrm.tip)
    kv("VRC tip", sim.vrc.tip)
    kv("epoch", sim.current_epoch_vrc())

    step("Attempt to build anchor — should fail and enter stale-coupling")
    a = sim.build_anchor_candidate()
    kv("anchor candidate built", a is not None)
    kv("stale reason", sim.stale.reason)

    step("Mark epoch boundary several times (sim ticks)")
    for _ in range(params.stale_grace_epochs + 1):
        sim.stale.on_epoch_boundary(advanced=False)
    kv("stall_epochs", sim.stale.stall_epochs)
    kv("warning level", sim.stale.warning_level(params))

    step("Mark several more epochs to cross S_MAX")
    for _ in range(params.stale_max_epochs - params.stale_grace_epochs):
        sim.stale.on_epoch_boundary(advanced=False)
    kv("stall_epochs", sim.stale.stall_epochs)
    kv("warning level (should be RecoveryEligible)", sim.stale.warning_level(params))

    step("VRM data returns: advance VRM to satisfy beacon")
    sim.mine_vrm(50)
    a2 = sim.build_anchor_candidate()
    kv("anchor candidate after recovery", a2 is not None)
    if a2 is not None:
        sim.stale.leave_stale()
        sim.stale.on_epoch_boundary(advanced=True)
    kv("stall_epochs after recovery", sim.stale.stall_epochs)
    kv("warning level after recovery", sim.stale.warning_level(params))

    print()
    print("  RESULT: chain entered stale-coupling, reached RecoveryEligible,")
    print("          then resumed once paired data returned. No consensus halt.")


if __name__ == "__main__":
    main()
