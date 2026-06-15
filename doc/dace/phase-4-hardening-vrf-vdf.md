# Phase 4 — Optional VRF/VDF Beacon Hardening Evaluation

Status: Draft — Evaluation only
Phase: 4 (post-mainnet hardening)
Prerequisite: Phase 3b mainnet active ≥ 12 months with sufficient telemetry.

## Objective

Evaluate whether DACE should adopt:

1. **VRF-based beacons** — replace the height-based deep beacon with a VRF-derived (verifiable random function) beacon that further constrains miner discretion.
2. **VDF gates** — gate beacon eligibility on a verifiable delay function evaluation to enforce a minimum wall-clock delay between beacons.
3. **Private self-reveal for committee membership** — instead of publicly listing the committee in `committee_root`, use a private-by-default scheme where members reveal their selection only when signing.

This is **evaluation only** in this document. Any decision to ship requires a full BIP cycle (DACE-8, DACE-9, ...) and a fresh phase plan analogous to Phases 0-3.

## Why this is not in Phase 1

The current DACE design intentionally avoids cryptographic novelty. Height-based deep beacons and public committees:

- Are simple and well-understood.
- Have a small attack surface that the audit firm can review thoroughly.
- Do not depend on libraries (VRF, VDF) whose own security depends on cryptographic assumptions still being settled by the academic community.

Adding VRF/VDF in Phase 1 would compound first-deployment risk with cryptographic novelty risk. The DACE-4 design guide explicitly defers this to Phase 4.

## Evaluation criteria

For each candidate hardening, the evaluator must produce a written report covering:

### 1. Threat model addressed

Which attacks does this hardening defend against that the current DACE design does not? Quantify residual attacks under the current design (using mainnet telemetry) before claiming improvement.

### 2. Implementation risk

- Library dependencies (libsodium VRF, libvdf, sigma protocols, etc.) and their maturity.
- New consensus-critical code surface.
- Backwards compatibility with existing DACE objects.

### 3. Performance impact

- Beacon validation cost increase.
- Committee selection cost increase.
- Block validation cost increase.
- All measured against current scrypt² baseline; must not approach mining hot-path costs.

### 4. Identity invariants

Does the hardening violate any Section 2.4 invariant? In particular:

- Does it require new VRM-side mining work? **If yes, evaluate against [powt-mining-philosophy.md](powt-mining-philosophy.md) Section 1.**
- Does it shift Verium security from "objective accumulated work" toward identity?
- Does it shift VeriCoin security from "bonded stake" toward live foreign reads?

### 5. Migration

- Activation height coordination.
- Operator upgrade burden.
- Wallet UX changes.

## Specific candidates

### VRF beacon

**Proposal**: replace `beacon_e.GetWorkHash()` mixing with a VRF output where the VRF input is `(VRM tip header, ticket operator private key)` and the VRF is verifiable against the ticket's `operator_pubkey`.

**Pros**: removes any residual beacon predictability for non-mining stakers.

**Cons**: introduces a VRF library dependency in consensus-critical paths; complicates the "objective accumulated work" property because the beacon value now depends on operator identity.

**Recommendation**: defer unless mainnet telemetry shows meaningful beacon-grinding attacks. The current design's deep-beacon requirement plus the kernel coupling already make beacon grinding equivalent in cost to predicting VRM PoW — which is already strong.

### VDF gate

**Proposal**: require a VDF evaluation that proves at least `T_VDF` seconds of sequential work has elapsed before the beacon becomes eligible. This bounds best-case beacon-grinding attack throughput regardless of attacker resources.

**Pros**: hardens against pathological-resource attacks (a hypothetical attacker with arbitrarily many VRM ASICs).

**Cons**: new cryptographic primitive in consensus; libvdf is still maturing; VDF parameters must be calibrated against current and future hardware; risks underestimating attacker capability.

**Recommendation**: monitor academic developments. Do not ship before VDF deployment elsewhere (e.g., Ethereum's eventual VDF integration) provides production data.

### Private self-reveal for committee

**Proposal**: replace the public `committee_root` with a cryptographic accumulator (e.g., Merkle tree of `H(ticket_id || epoch_secret)`). Operators reveal their inclusion only at signing time. This frustrates bribery and targeted attacks during the FINALITY_WINDOW.

**Pros**: reduces public predictability of who can be bribed.

**Cons**: complicates slashing evidence (the accumulator design must still support proving equivocation); changes the wire format of JointAnchor.

**Recommendation**: most promising of the three. Worth a detailed BIP draft (DACE-8) after Phase 3b clean year. The Ethereum ecosystem's draft EIP-7998 work is directly relevant; track its progress.

## Decision process

For each candidate:

1. **Year 1 post-mainnet**: collect data. Do not propose changes during the stabilization window.
2. **Year 2**: if telemetry justifies, commission an external evaluation (the same audit firm as Phase 2c, or a different one for fresh eyes).
3. **Year 2-3**: if external evaluation is positive, publish a DACE-8/9/10 BIP and follow the same phase cycle as DACE v1.

No hardening ship before the year-2 milestone except in response to a Critical operationally-confirmed vulnerability.

## Out of scope for this document

- Multi-resource consensus (Minotaur-style PoW+PoS merge): explicitly rejected as it would abandon the Binary Chain identity.
- External notary layer (Komodo-style dPoW): explicitly rejected per design guide Section 16.1.
- L2 settlement layers: orthogonal to consensus; not a DACE concern.
