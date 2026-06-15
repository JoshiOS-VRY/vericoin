# Binary Chain v2 — Codebase Mapping and RPCs

> Honors [`12-consensus-boundary.md`](12-consensus-boundary.md). `vericoin/src/dace/` stays
> consensus-safe only. Jackpot eligibility/exclusion/USD/winner-selection live in
> explorer/indexer/app. Binary Address lives in the wallet/library layer.

## 1. Layered file map

### Layer 1 — Consensus / node (`vericoin/src/`)

| Area | Files | v2 work |
|------|-------|---------|
| Consensus params | `consensus/params.h`, `chainparams.cpp` | Add degradation thresholds + σ/φ-modulation rule. **No jackpot constants.** Freeze `BinaryChainHeight*`, `DaceBootstrap*`. |
| Extended header | `primitives/block.{h,cpp}`, `dace/header.{h,cpp}` | Already implemented; verify hashing/round-trip. |
| Validation | `validation.cpp` (`CheckExtendedHeader`, `OnConnectBlock`, `OnDisconnectBlock`) | Enforce `ValidateClaim`; apply degradation multiplier; wire beacon/committee. |
| Beacon | `dace/beacon.{h,cpp}` | Wire `SetSelectedBeacon`/`SelectBeacon` into connect flow. |
| Committee | `dace/ticket_registry.{h,cpp}`, `dace/sortition.{h,cpp}` | Wire `SampleCommittee`; persist registry (`LoadFromDisk`/`Flush`). |
| Anchor lifecycle | `dace/anchor_lifecycle.{h,cpp}`, `dace/activation.*`, `dace/recovery.*` | Invoke `Certify`; wire `PromoteIfActivated`, `Rewind`. |
| Rewards | `dace/reward_accumulator.{h,cpp}`, `dace/claim.{h,cpp}` | Enforce claims; apply σ/φ multiplier; fill `recipient_pkh`. |
| **Degradation (new)** | `dace/degradation.{h,cpp}` | Overlay on `dace/stale_coupling.*`; pure function of canonical `consecutive_missed_anchors` → σ/φ multiplier. |
| Paired headers | `dace/paired_view.{h,cpp}` | Add `OtherChainHeaderDB` persistence (LevelDB `xheaders/`). |
| P2P | `net_processing.cpp`, `dace/net_messages.{h,cpp}` | Implement `getxheaders` serving, `getja`, `jasig` assembly. |
| Kernel | `dace/kernel.{h,cpp}`, `pos.cpp` (`ComputeKernelMix`) | Replace Phase-1 mix with full `hashCrossChain` once anchors flow. |
| Miner/staker | `miner.cpp` | Fill extended header fields; include certified JA in coinbase (`OP_JA`). |
| Init/RPC reg | `init.cpp`, `rpc/register.h` | `Service::Initialize`/`Teardown`; `RegisterDaceRPCCommands`. |

### Layer 1 — Verium side (does not exist yet)

| Area | Files | v2 work |
|------|-------|---------|
| **Verium DACE (new)** | `verium/src/dace/` (mirror of VeriCoin's) | Beacon production, extended header, coinbase JA inclusion (`OP_JA`), paired-header ingestion of VeriCoin headers, `consensus/params.h` DACE constants. **Largest gap; Binary Chain is not binary until this ships.** |

### Layer 3 — Wallet / library

| Area | Files | v2 work |
|------|-------|---------|
| Binary Address | new `binary_address.{h,cpp}` (or a small shared lib) reusing `key_io.h` bech32 | `vbc1` encode/decode; wallet RPCs. |
| Wallet claim auto-redeem | wallet code | Scan accumulator roots, build `ClaimRedemptionWitness`, broadcast. |

### Layer 4 — Explorer / indexer (`vericonomyexplorer-v2/`)

| Area | Files | v2 work |
|------|-------|---------|
| Jackpot eligibility | new indexer module | Build participant set, contribution scores from accepted chain data. |
| Exclusion registry | indexer/app config + service | Pool/exchange/custodial clustering; **app policy, not consensus.** |
| Reward-event calc | indexer | Compute seed, weighting, winner; publish recomputable proof. |
| USD display | explorer-web | Estimated USD only; clearly labeled. |
| Binary Chain surfaces | `BinaryChainHero.tsx`, `BinaryChainActivity.tsx` (currently dead code — wire it), new degradation/jackpot panels | Show anchor state, degradation, pool (coin + est USD). |

### Layer 5 — App / foundation promotion

| Area | Where | v2 work |
|------|-------|---------|
| Promotion service | off-chain / foundation infra | Runs the event, applies exclusions, publishes winner + proof, pays from multisig treasury. |
| Jackpot config | app/indexer config + docs | Epoch length (90d), split (85/15), funding rules. **Never `consensus/params.h`.** |

### Layer 3/UI — Desktop app (`verium/desktop/verium-app/`)

| Area | Files | v2 work |
|------|-------|---------|
| DACE dashboard | `src/pages/BinaryChain.tsx`, `src/lib/rpc/dace.ts`, `src-tauri/src/dace_commands.rs` | Add degradation + jackpot-status + participation panels; surface Binary Address. |
| Dual-daemon | `src-tauri/src/coin_profile.rs` | Already manages both daemons; extend for cross-daemon claim/payout UX. |

## 2. RPCs

### Existing (`rpc/dace.cpp`, category `dace`)

`binarychain_status`, `binarychain_metrics`, `binarychain_anchor`,
`binarychain_register_ticket`, `binarychain_unbond_ticket`, `binarychain_redeem_claim`,
`binarychain_fund_wallet` (binarytest only).

### New — node policy / RPC (layer 2)

| RPC | Returns | Notes |
|-----|---------|-------|
| `binarychain_degradation` | ladder state, σ/φ multipliers, recovery flags, node-local freshness block | Derived from canonical chain state; freshness clearly labeled node-local |
| `binarychain_reward_accumulators` | per-epoch accumulator roots, escrow balances, pending/claimed | Canonical |
| `binarychain_participation` | raw, canonical per-address participation data (work/stake-time/uptime proofs) | **Raw feed only — not winner selection** |

### New — wallet / library (layer 3)

| RPC | Returns |
|-----|---------|
| `getbinaryaddress` | fresh `vbc1…` for this wallet |
| `decodebinaryaddress` | `{ vrc_address, vrm_address, version, flags, identity_id? }` |

### Naming note on the spec's suggested RPCs

The original request listed `getbinarychainstatus`, `getanchorstate`, `getotherchainheaders`,
`submitotherheader`, `getrewardaccumulators`, `getbinaryjackpotstatus`, `getjackpoteligibility`,
`getbinaryaddress`, `decodebinaryaddress`. The codebase already uses the `binarychain_*`
convention, so map them as:

| Requested | Use existing/new |
|-----------|------------------|
| `getbinarychainstatus` | `binarychain_status` (existing) |
| `getanchorstate` | `binarychain_anchor` (existing) + state from `binarychain_status` |
| `getotherchainheaders` | `binarychain_otherheaders` (new, reads `OtherChainHeaderDB`) |
| `submitotherheader` | handled via P2P `xheaders`; debug RPC `binarychain_submitotherheader` (regtest) |
| `getrewardaccumulators` | `binarychain_reward_accumulators` (new) |
| `getbinaryjackpotstatus` | **explorer/indexer/app endpoint**, not a daemon RPC (layer 4/5) |
| `getjackpoteligibility` | **explorer/indexer/app endpoint** built from `binarychain_participation` |
| `getbinaryaddress` / `decodebinaryaddress` | wallet RPCs (layer 3) |

> Jackpot status/eligibility are **not** daemon consensus RPCs in v2. The daemon exposes raw
> participation data; the indexer/app computes and serves jackpot status and eligibility.

## 3. Consensus vs app-layer — one-line rules

- If it changes block/tx validity or reward roots → `vericoin/src/dace/` (+ Verium mirror).
- If it is a warning/metric/raw feed → `rpc/dace.cpp` (layer 2), never feeds roots.
- If it encodes/decodes addresses → wallet/library (layer 3).
- If it scores participants, excludes addresses, values in USD, or picks a winner →
  explorer/indexer/app (layer 4/5), never consensus.
