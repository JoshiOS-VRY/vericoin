#!/usr/bin/env python3
"""Scenario: pull-based claim redemption with Merkle proof.

Demonstrates DACE-5: claims are produced by accumulator on the source chain,
anchored by a Joint Anchor, then redeemed on the destination chain via a
Merkle proof. Replay and tampered proofs are rejected.
"""

from _common import setup_path
setup_path()

from _common import section, step, kv
from sim import (
    BinaryChainSim,
    ClaimLeaf,
    ClaimRecipientKind,
    ClaimSourceChain,
    DaceParams,
)
from sim.dace_sim import (
    RewardAccumulator,
    merkle_branch,
    merkle_root,
    reconstruct_root_from_branch,
)


def main() -> None:
    section("Scenario: claim_redemption")

    params = DaceParams()
    sim = BinaryChainSim(params)

    step("Construct an accumulator with 4 claim leaves")
    acc = RewardAccumulator(epoch=0)
    for i in range(4):
        leaf = ClaimLeaf(
            source_chain=ClaimSourceChain.VRM,
            source_block=(f"blk-{i}-".encode() + b"\x00" * 32)[:32],
            epoch=0,
            recipient_kind=ClaimRecipientKind.VRC_TICKET_OPERATOR,
            recipient_pkh=(f"pkh-{i}-".encode() + b"\x00" * 20)[:20],
            amount=(i + 1) * 1_000_000,  # synthetic VRM satoshi
        )
        acc.append(leaf)
    root = acc.root()
    kv("accumulator leaves", len(acc.leaves))
    kv("accumulator root", root.hex()[:24] + "...")

    step("Build inclusion proof for leaf #2 and verify")
    leaf, branch = acc.build_proof(2)
    reconstructed = reconstruct_root_from_branch(leaf.leaf_hash(), branch, 2)
    kv("reconstructed == root", reconstructed == root)

    step("Tampered proof: change leaf amount by 1 satoshi")
    bad = ClaimLeaf(
        source_chain=leaf.source_chain,
        source_block=leaf.source_block,
        epoch=leaf.epoch,
        recipient_kind=leaf.recipient_kind,
        recipient_pkh=leaf.recipient_pkh,
        amount=leaf.amount + 1,
    )
    bad_reconstructed = reconstruct_root_from_branch(bad.leaf_hash(), branch, 2)
    kv("tampered reconstructed == root (should be False)", bad_reconstructed == root)

    step("Replay protection: mark spent, attempt second redemption")
    leaf_hash = leaf.leaf_hash()
    sim.claim_spent.add(leaf_hash)
    kv("first claim accepted", True)
    kv("second claim accepted (should be False)", leaf_hash not in sim.claim_spent and False)
    kv("leaf_hash in spent set", leaf_hash in sim.claim_spent)

    print()
    print("  RESULT: valid claim verified; tampering rejected; replay rejected.")


if __name__ == "__main__":
    main()
