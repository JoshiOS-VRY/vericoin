# Binary Chain v2 — Binary Address

> **Wallet / library layer (layer 3).** Consensus does not need to know about Binary
> Address in v2. See [`12-consensus-boundary.md`](12-consensus-boundary.md).

## 1. Problem

If a Binary Chain Reward Event (or any cross-chain bonus) pays both VRC and VRM, the
recipient should not have to manually manage two separate receiving addresses. We want one
user-facing descriptor that resolves to a native VeriCoin address and a native Verium
address.

## 2. What Binary Address is (and is not)

A **Binary Address** is a Bech32m-style **dual-chain descriptor** that commits to one
VeriCoin receiving address/key and one Verium receiving address/key.

It is **not**:

- a third coin;
- a bridge;
- a third blockchain;
- a consensus object.

It **is** a user-facing wrapper that decodes to two native addresses. Payout software
decodes it and sends native VRC to the VRC address and native VRM to the VRM address.

```text
vbc1...   ->   { vrc_address: "v...",  vrm_address: "V..." }
```

## 3. Derivation (same seed, separate paths, separate native addresses)

```text
seed (BIP39 mnemonic)
 ├── m / 44' / <coin_type_VRC>' / account' / change / index   -> vrc_key -> vrc_address
 └── m / 44' / <coin_type_VRM>' / account' / change / index   -> vrm_key -> vrm_address

Binary Address = encode(version, network, commit(vrc_address, vrm_address))
```

- **Same seed**, so one backup recovers both.
- **Separate BIP32 derivation paths** (distinct SLIP-44 coin types for VRC and VRM), so the
  two native addresses are independent and standard.
- **Separate native addresses**, fully valid on their own chains.
- **One Binary Address wrapper** for UX.

## 4. Encoding

Bech32m (BIP-350, the spec used for Taproot/witness v1+; chosen because it fixes the
Bech32 insertion-mutation weakness for non-zero witness versions).

```text
HRP (human-readable part):
  vbc      mainnet
  tvbc     testnet
  rvbc     regtest / binarytest

separator: '1'

data part (5-bit groups, Bech32m-checksummed):
  version       (1 byte)   // Binary Address format version, starts at 0x01
  flags         (1 byte)   // bit0: vrc present, bit1: vrm present, bit2: identity present
  vrc_payload   (var)      // canonical encoding of the VRC receiving address (see 4.1)
  vrm_payload   (var)      // canonical encoding of the VRM receiving address
  [identity_id] (var)      // optional Binary Identity reference (doc 05 sec 6)
  checksum      (6 chars)  // Bech32m
```

### 4.1 Payload options (pick one at Phase 0; recommended: A)

- **Option A — address-commit (recommended for v1).** `vrc_payload` / `vrm_payload` carry
  the *witness program / hash* of each native address (e.g. the 20-byte P2PKH/P2WPKH hash
  + a 1-byte script-type tag). Decoders reconstruct the exact native address string using
  each chain's existing `key_io` rules. Simple, no key exposure, no on-chain footprint.
- **Option B — pubkey-commit.** Carry compressed pubkeys and derive addresses on decode.
  Larger; exposes pubkeys earlier; only useful if descriptor-style watch-only is desired.

### 4.2 Checksum, version, network prefix

- **Checksum:** Bech32m 6-character checksum over HRP + data; reject on mismatch.
- **Version:** 1-byte format version (`0x01`) enables future layouts without ambiguity.
- **Network prefix:** the HRP (`vbc`/`tvbc`/`rvbc`) prevents cross-network misuse; decoders
  refuse an address whose HRP does not match the active network.

## 5. Decode / payout flow (payout software)

```text
decodeBinaryAddress(s):
  1. Bech32m-decode s; verify checksum; split HRP/data.
  2. Verify HRP matches active network.
  3. Read version; if unknown -> reject (forward-compat: do not guess).
  4. Read flags; require vrc and vrm present for a payout target.
  5. Reconstruct vrc_address via VeriCoin key_io rules; vrm_address via Verium key_io rules.
  6. Return { vrc_address, vrm_address, identity_id? }.

payout(binary_address, vrc_amount, vrm_amount):
  d = decodeBinaryAddress(binary_address)
  sendOnVericoin(d.vrc_address, vrc_amount)   // native VRC
  sendOnVerium(d.vrm_address, vrm_amount)     // native VRM
```

Two independent native transactions on two independent chains. No bridge, no wrapping.

## 6. Recovery behavior

- One BIP39 seed backs up both native key trees, so a single mnemonic restores both
  addresses and therefore the Binary Address.
- The Binary Address itself is **derived data**, not a secret; it can be regenerated from
  the two native addresses at any time.
- If only one native address is known, a partial Binary Address (flags indicating one
  chain) is still valid for single-chain receipt.

## 7. Privacy implications

- A Binary Address **links** a user's VRC and VRM addresses publicly to anyone who has the
  Binary Address. This is a deliberate UX/privacy trade-off.
- Mitigations: support generating **per-use** Binary Addresses (fresh derivation indices),
  so a published Binary Address does not reveal a user's entire holdings; and keep the
  default wallet behavior to rotate indices.
- For jackpot payout, a one-time Binary Address can be registered with the promotion so the
  winner's main addresses are not exposed.

## 8. Consensus vs wallet boundary

- **v1: wallet/app-layer only.** Consensus, the daemons, and DACE are entirely unaware of
  Binary Address. It is encode/decode in a wallet library + two RPCs.
- The library reuses each chain's existing `key_io.h` Bech32/Base58 utilities; no consensus
  code changes.
- **Future (Phase 4, optional):** if there is ever a reason for consensus to recognize a
  Binary Address (e.g. a protocol-native dual payout), it would require its own BIP and
  gating; **explicitly out of scope for v2.**

## 9. RPCs (wallet layer)

| RPC | Layer | Returns |
|-----|-------|---------|
| `getbinaryaddress` | wallet | A fresh `vbc1…` wrapping a new VRC + new VRM receiving address from this wallet's seed |
| `decodebinaryaddress` | wallet/library | `{ vrc_address, vrm_address, version, flags, identity_id? }` |

Both live at the wallet/`key_io` layer, not in `vericoin/src/dace/`.

## 10. Required tests

- Round-trip: `getbinaryaddress` → `decodebinaryaddress` reproduces the exact native
  addresses on all three networks.
- Bech32m checksum rejects single/double character mutations.
- HRP mismatch (mainnet address on testnet) is rejected.
- Unknown version is rejected, not guessed.
- Payout sends correct native amounts to both chains in a regtest two-daemon harness.
