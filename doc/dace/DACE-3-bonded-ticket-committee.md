# DACE-3: Bonded Ticket Registry and Committee Sortition

Status: Draft
Type: Standards Track (consensus, hard fork)
Created: 2026-05-24

## Abstract

DACE-3 defines a fixed-size bonded ticket registry on the VeriCoin chain and a deterministic without-replacement sortition rule that selects per-epoch committees from it. Tickets are stake units, not identities. Splitting a stake into multiple tickets yields linear influence, never superlinear.

## Motivation

v2 picked committees from identity proxies ("recent distinct VRM coinbase scripts" plus "top-weight recent VRC stake outputs"). Both proxies are gameable. Bonded tickets bind influence to staked capital, not on-chain identity.

## Specification

### Ticket registry

The registry is a deterministic on-chain set maintained by VRC consensus. Each ticket entry is:

```
struct Ticket {
    COutPoint stake_outpoint;   // funding output, locked for L epochs
    CPubKey   operator_pubkey;  // committee signing key
    uint32_t  registered_epoch; // epoch in which this ticket became active
    uint256   ticket_id;        // = H(stake_outpoint || operator_pubkey)
}
```

Lockup `L` (per [DACE-0](DACE-0-overview-and-constants.md)) means the funding output cannot be spent except through the registry's `unbond` mechanism.

### Transactions

Two new transaction types (encoded as standard VRC transactions with marker outputs in the coinbase or as standalone tx kinds; concrete encoding in the reference implementation):

#### register_ticket

Inputs:
- One unspent VRC output of exactly `STAKE_UNIT_VRC` value.

Outputs:
- A locked output paying back to a P2SH-style "ticket script" that encumbers the funds until unbonding completes.
- A registry-marker output of zero value carrying the operator pubkey and a registration declaration.

Validation:
- The funding output value MUST equal `STAKE_UNIT_VRC` exactly. Off-amount registrations are rejected.
- The operator pubkey MUST be unique within the active registry.
- The transaction becomes effective at the start of the next epoch (`registered_epoch = current_epoch + 1`).

#### unbond_ticket

Inputs:
- The locked output of an active or previously-active ticket.

Outputs:
- A time-locked output that becomes spendable after `UNBOND_DELAY` epochs.

Validation:
- The ticket MUST exist in the active registry or in the unbonding set.
- Unbonding a ticket removes it from sortition starting the next epoch.
- Unbonded funds become spendable only after `UNBOND_DELAY` to allow slashing windows to elapse.

### Ticket registry root

Every VRC block at height `h >= BinaryChainHeight_VRC` carries the Merkle root of the *active* ticket set:

```
ticket_root = MerkleRoot(sorted(ticket_id for ticket in active_registry))
```

This root is committed in the block's coinbase scriptSig as a marker push. It is referenced by Joint Anchors (see [DACE-4](DACE-4-joint-anchors.md)) as `committee_root`. Sorting is by lexicographic order of `ticket_id`.

### Sortition

For epoch `e`, the sortition seed is:

```
S_e = H(JA_{e-1}.hash || beacon_e.hash || VRC_checkpoint_{e-1}.hash)
```

Per-ticket score:

```
score_i = H(S_e || ticket_id_i)
```

The committee for epoch `e` is the `M` tickets with the lowest scores, **without replacement**:

```
committee_e = first M tickets in sorted-by-score order
```

Ties are broken by lexicographic order of `ticket_id` (statistically negligible at 256-bit scores).

### Slashing

Two slashable offences:

1. **Equivocation**: an operator signs two distinct Joint Anchors for the same epoch.
2. **Conflicting recovery**: an operator signs a conflicting recovery anchor.

Slashing evidence is a single VRC transaction containing both signed objects. Upon inclusion:
- The offender's locked stake is burned (sent to an unspendable script).
- The offender's ticket is removed from the registry.

Slashing has a `SLASH_WINDOW` of `L + UNBOND_DELAY` epochs from the offence epoch. After that, evidence is too old to act on.

## Rationale

### Why fixed-size tickets

Variable-size tickets reintroduce a Sybil surface: an attacker who splits a large stake into `N` small tickets in a count-based committee gains `N` seats. Fixed-size tickets cap this — each `STAKE_UNIT_VRC` buys exactly one ticket whether you have 1× or 100× the unit.

### Why without-replacement

With replacement, a large bonded address gets multiple seats per epoch proportionally to its ticket count. Without replacement, you cap a single ticket at one seat per epoch, and a large operator's influence scales by ticket count linearly without any "lucky draws" bonus.

### Why operator_pubkey separate from stake_outpoint

Separation lets an operator rotate signing keys without unbonding capital, and lets a custodian delegate signing to a hot key without exposing the cold-stored funding key.

## Reference implementation

- [vericoin/src/dace/ticket_registry.h](../../src/dace/ticket_registry.h)
- [vericoin/src/dace/ticket_registry.cpp](../../src/dace/ticket_registry.cpp)
- [vericoin/src/dace/sortition.h](../../src/dace/sortition.h)
- [vericoin/src/dace/sortition.cpp](../../src/dace/sortition.cpp)

## Test vectors

`vericoin/test/dace/sortition_vectors.json` covers:
- Single-ticket registry: that ticket is always selected.
- Many-ticket registry: stochastic distribution matches uniform sampling without replacement.
- Splitting test: an attacker splitting 100× `STAKE_UNIT_VRC` into 100 tickets gets ~100×N/total expected seats, not more.
- Slashing evidence: equivocation produces a valid slash; the slashed ticket is removed in the following epoch.
