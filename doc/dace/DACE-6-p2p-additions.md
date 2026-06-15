# DACE-6: P2P Protocol Additions

Status: Draft
Type: Standards Track
Created: 2026-05-24

## Abstract

DACE-6 adds new P2P messages to relay paired-chain headers, Joint Anchors, anchor signatures, and (optionally) claim lookups. Defines `BC_PROTOCOL_VERSION` and the post-activation peer compatibility rule.

## Specification

### Protocol version

`BC_PROTOCOL_VERSION = 90100`.

- Below activation: tolerated.
- At or above activation: peers below `BC_PROTOCOL_VERSION` may be served limited requests but are not relayed new DACE objects.
- After `T+7 days` from activation: peers below `BC_PROTOCOL_VERSION` are refused.

### New messages

All messages are chain-id prefixed (1 byte): `0x76 = vericoin`, `0x77 = verium`. This allows a node tracking both chains' headers to disambiguate.

#### getxheaders

Request paired-chain headers.

```
struct GetXHeaders {
    uint8_t  chain_id;        // which chain's headers are requested
    uint256  block_locator;   // standard locator semantics
    uint256  hash_stop;       // standard stop semantics
}
```

#### xheaders

Response carrying paired-chain headers.

```
struct XHeaders {
    uint8_t  chain_id;
    vector<BlockHeader_v3> headers;
}
```

Receivers append validated headers to their paired-chain header set per [DACE-7](DACE-7-ibd-and-stale-coupling.md).

#### getja

Request a Joint Anchor by hash.

```
struct GetJA {
    uint256 ja_hash;
}
```

#### ja

Response carrying a Joint Anchor.

```
struct JA {
    JointAnchor anchor;          // see DACE-4
}
```

#### jasig

Gossip a single committee signature during the assembly window.

```
struct JASig {
    uint32_t   epoch_index;
    uint256    ja_partial_hash;  // hash over non-signature fields
    uint16_t   committee_index;
    Signature  sig;
}
```

Receivers aggregate sigs locally; once `q` is reached they construct the certified `JointAnchor` and gossip via `ja`.

#### getclaim and claim (optional, convenience)

Wallets can request known claims for a recipient pkh.

```
struct GetClaim {
    uint160 recipient_pkh;
    uint32_t since_epoch;
}

struct Claim {
    vector<ClaimRedemptionWitness> claims;   // each with leaf + merkle_path + ja_hash
}
```

This is a wallet convenience, not a consensus requirement; nodes MAY refuse to serve it.

### Misbehavior scoring

Standard scoring patterns from [vericoin/src/net_processing.cpp](../../src/net_processing.cpp) apply:

| Offense | Score |
|---------|------:|
| Invalid xheader (PoW/PoST check fails) | 50 |
| Invalid JA signature aggregation | 100 |
| Sending a `ja` for an unknown beacon | 20 |
| Spam: `getxheaders` rate-limit exceeded | 10 per excess request |

### P2P magic bytes cleanup (optional)

VRM and VRC currently share message-start magic `0x70 0x35 0x22 0x05` and are distinguished only by port (see [vericoin/src/chainparams.cpp](../../src/chainparams.cpp) and [verium/src/chainparams.cpp](../../../verium/src/chainparams.cpp)). DACE optionally proposes:

- VRM: `0x42 0x43 0x56 0x4D` ("BCVM")
- VRC: `0x42 0x43 0x56 0x43` ("BCVC")

Inclusion in DACE activation is a governance decision; it is cosmetic but addresses a long-standing quirk.

## Reference implementation

- [vericoin/src/dace/net_messages.h](../../src/dace/net_messages.h)
- [vericoin/src/dace/net_messages.cpp](../../src/dace/net_messages.cpp)
- Hooks into [vericoin/src/net_processing.cpp](../../src/net_processing.cpp)

## Test vectors

`vericoin/test/dace/p2p_vectors.json` covers:
- Round-trip serialization of each new message type.
- Rate-limit enforcement.
- Misbehavior scoring on invalid payloads.
