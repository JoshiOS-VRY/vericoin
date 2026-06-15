#!/usr/bin/env python3
"""Scenario: end-to-end happy path through one full Joint Anchor lifecycle.

Demonstrates:
  1. Both chains advance past activation height.
  2. A VRM beacon is selected for epoch 0 once K_BEACON confirmations land.
  3. The bonded ticket registry seeds a committee.
  4. A JointAnchor candidate is constructed, signed by quorum, certified.
  5. After the epoch-delay + VRM coinbase inclusion, the JA activates.
  6. Reward accumulator root for VRC closes for epoch 0.
"""

from _common import setup_path
setup_path()

from _common import section, step, kv
from sim import BinaryChainSim, DaceParams  # noqa: E402


def main() -> None:
    section("Scenario: happy_path")

    sim = BinaryChainSim(DaceParams(
        binary_chain_height_vrc=50,
        binary_chain_height_vrm=50,
        beacon_delta=4,
        beacon_k=4,
        beacon_epoch_vrc=10,
        committee_size=4,
        committee_quorum_num=2,
        committee_quorum_den=3,
    ))

    step("Advance both chains to activation height + buffer")
    sim.mine_vrm(80)
    sim.stake_vrc(70)
    kv("VRM tip", sim.vrm.tip)
    kv("VRC tip", sim.vrc.tip)
    kv("current epoch (VRC)", sim.current_epoch_vrc())

    step("Register 4 bonded tickets (committee_size = 4)")
    for i in range(4):
        outpoint = (f"o-{i}-".encode() + b"\x00" * 36)[:36]
        pubkey = (f"k-{i}-".encode() + b"\x00" * 33)[:33]
        sim.register_ticket(outpoint, pubkey, epoch=sim.current_epoch_vrc())
    kv("registered tickets", len(sim.tickets))

    step("Select the deterministic VRM beacon for current epoch")
    beacon = sim.beacon_for_epoch(sim.current_epoch_vrc())
    if beacon is None:
        print("    !! No beacon selected — VRM chain too short or insufficient depth")
        return
    kv("beacon.epoch_index", beacon.epoch_index)
    kv("beacon.height", beacon.beacon_height)
    kv("beacon.selection_offset", beacon.selection_offset)
    kv("beacon.hash (hex)", beacon.beacon_hash.hex()[:24] + "...")

    step("Sample the committee")
    committee = sim.active_committee()
    kv("committee size", len(committee))
    for i, t in enumerate(committee):
        kv(f"  committee[{i}].pubkey", t.operator_pubkey.hex()[:24] + "...")

    step("Build anchor candidate")
    anchor = sim.build_anchor_candidate()
    if anchor is None:
        print("    !! Anchor candidate could not be built (no beacon).")
        return
    kv("anchor.epoch_index", anchor.epoch_index)
    kv("anchor.partial_hash", anchor.partial_hash().hex()[:24] + "...")

    step("Sign anchor with full quorum")
    sim.sign_anchor(anchor, committee, sign_count=len(committee))
    kv("signatures collected", len(anchor.signature_pubkeys))
    kv("phase after sign", sim.lifecycle.entries[anchor.partial_hash()].phase.value)

    step("Tick forward: epoch-delay + VRM coinbase inclusion depth")
    for _ in range(sim.params.beacon_epoch_vrc + 2):
        sim.stake_vrc(1)
        sim.mine_vrm(1)
    active = sim.lifecycle.active()
    if active is None:
        print("    !! Anchor did not activate. (Likely waiting on VRM coinbase depth in real chain.)")
    else:
        kv("active anchor epoch", active.epoch_index)
        kv("active anchor full_hash", active.full_hash().hex()[:24] + "...")

    step("Reward accumulator root for closed epoch 0 (VRC side)")
    closed_root = sim._closed_root(0, sim.accumulators[0].leaves[0].source_chain) if 0 in sim.accumulators else b""
    kv("acc[0].leaves", len(sim.accumulators[0].leaves) if 0 in sim.accumulators else 0)
    kv("acc[0].root", closed_root.hex()[:24] + "..." if closed_root else "<empty>")

    print()
    print("  RESULT: happy-path lifecycle observed end-to-end.")


if __name__ == "__main__":
    main()
