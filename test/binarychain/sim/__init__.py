"""
Binary Chain v3 (DACE) Python simulation library.

A standalone Python implementation of every DACE algorithm, matching the
C++ reference at vericoin/src/dace/ byte-for-byte. Useful for:

  - Exercising DACE end-to-end without compiling the C++ daemon.
  - Cross-checking the C++ implementation against test vectors.
  - Demoing scenarios to reviewers without standing up a regtest network.

The simulation never touches mainnet. It runs purely in-process.
"""

from .dace_sim import (
    Beacon,
    BinaryChainSim,
    ClaimLeaf,
    ClaimSourceChain,
    ClaimRecipientKind,
    DaceParams,
    JointAnchor,
    Ticket,
    select_beacon,
    sortition_seed,
    sample_committee,
    ticket_id,
    ticket_score,
)

__all__ = [
    "Beacon",
    "BinaryChainSim",
    "ClaimLeaf",
    "ClaimSourceChain",
    "ClaimRecipientKind",
    "DaceParams",
    "JointAnchor",
    "Ticket",
    "select_beacon",
    "sortition_seed",
    "sample_committee",
    "ticket_id",
    "ticket_score",
]
