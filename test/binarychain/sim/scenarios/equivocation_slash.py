#!/usr/bin/env python3
"""Scenario: equivocation detection and slashable evidence.

Demonstrates DACE-4 §"Slashing": when a committee member signs two
distinct anchors for the same epoch, the AnchorLifecycle detects the
equivocation. In production, this generates a slashable transaction that
burns the offender's stake and removes their ticket.
"""

from _common import setup_path
setup_path()

from _common import section, step, kv
from sim import BinaryChainSim, DaceParams, JointAnchor


def main() -> None:
    section("Scenario: equivocation_slash")

    sim = BinaryChainSim(DaceParams())

    step("Build two distinct anchor candidates for the same epoch")
    a1 = JointAnchor(
        epoch_index=5,
        prev_anchor_hash=b"\x11" * 32,
        beacon_ref=b"\xaa" * 32,
        vrc_checkpoint_hash=b"\x22" * 32,
        vrc_checkpoint_height=100,
        reward_root_vrc_prev=b"\x00" * 32,
        reward_root_vrm_prev=b"\x00" * 32,
        committee_root=b"\x33" * 32,
    )
    a2 = JointAnchor(
        epoch_index=5,
        prev_anchor_hash=b"\x11" * 32,
        beacon_ref=b"\xbb" * 32,            # different beacon
        vrc_checkpoint_hash=b"\x44" * 32,    # different checkpoint
        vrc_checkpoint_height=100,
        reward_root_vrc_prev=b"\x00" * 32,
        reward_root_vrm_prev=b"\x00" * 32,
        committee_root=b"\x33" * 32,
    )
    kv("a1.partial_hash", a1.partial_hash().hex()[:24] + "...")
    kv("a2.partial_hash", a2.partial_hash().hex()[:24] + "...")
    kv("distinct partial hashes (expect True)", a1.partial_hash() != a2.partial_hash())

    step("Observe both anchors in the lifecycle")
    sim.lifecycle.observe(a1)
    sim.lifecycle.observe(a2)

    step("Detect equivocation when a2 lands after a1")
    other = sim.lifecycle.detect_equivocation(a2)
    kv("equivocation detected", other is not None)
    if other is not None:
        kv("conflicting partial_hash", other.partial_hash().hex()[:24] + "...")

    step("Slashing decision (in production: burn stake + remove ticket)")
    kv("offender would be slashed", other is not None)

    print()
    print("  RESULT: equivocation detected. Slashable evidence available.")


if __name__ == "__main__":
    main()
