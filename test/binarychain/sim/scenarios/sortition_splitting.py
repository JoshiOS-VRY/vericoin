#!/usr/bin/env python3
"""Scenario: stake-splitting attack resistance under without-replacement sortition.

Demonstrates DACE-3 §"Rationale": an attacker who splits one large stake into
N small tickets cannot gain superlinear influence — their expected seat share
matches `N / total`, not anything higher.
"""

from _common import setup_path
setup_path()

from _common import section, step, kv
from sim import (
    BinaryChainSim,
    DaceParams,
    sample_committee,
    sortition_seed,
    Ticket,
)


def main() -> None:
    section("Scenario: sortition_splitting")

    params = DaceParams(committee_size=8)
    sim = BinaryChainSim(params)

    step("Build a registry of 100 tickets")
    registry = []
    for i in range(100):
        outpoint = (f"o-{i}-".encode() + b"\x00" * 36)[:36]
        pubkey = (f"k-{i}-".encode() + b"\x00" * 33)[:33]
        registry.append(Ticket(
            stake_outpoint=outpoint,
            operator_pubkey=pubkey,
            registered_epoch=0,
            stake_amount=params.ticket_stake_unit,
        ))
    kv("registry size", len(registry))

    step("Mark the first 10 tickets as 'attacker' (split bonded stake)")
    attacker = set((t.operator_pubkey for t in registry[:10]))

    step("Run 1000 sortitions with different seeds; measure attacker seat share")
    total_attacker_seats = 0
    N = 1000
    for i in range(N):
        seed = sortition_seed(
            i.to_bytes(32, "little"),
            b"\x00" * 32,
            b"\x00" * 32,
        )
        committee = sample_committee(registry, seed, params.committee_size)
        for t in committee:
            if t.operator_pubkey in attacker:
                total_attacker_seats += 1
    avg = total_attacker_seats / N
    expected = 10 / 100 * params.committee_size  # = 0.8
    kv("expected (linear)", f"{expected:.3f}")
    kv("observed average", f"{avg:.3f}")
    kv("ratio observed/expected", f"{avg / expected:.3f}")

    if 0.7 <= avg <= 0.9:
        print()
        print("  RESULT: attacker influence is linear in ticket count, NOT superlinear.")
    else:
        print()
        print(f"  RESULT: anomalous attacker share {avg:.3f} — investigate.")


if __name__ == "__main__":
    main()
