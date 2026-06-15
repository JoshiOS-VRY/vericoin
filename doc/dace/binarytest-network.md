# Binarytest Network — Isolated DACE Test Environment

Status: Stable test infrastructure
Audience: Developers, testers, auditors evaluating Binary Chain v3

## Purpose

`binarytest` is the named isolated test network for Binary Chain v3 (DACE). It is structurally separate from mainnet at the wire, address, port, and datadir layers so that:

- A binarytest node physically cannot peer with a mainnet vericoin / verium node.
- Addresses and keys generated under binarytest cannot be cross-pasted into mainnet wallets.
- Datadirs do not collide with any production install.

## Isolation summary

| Layer | Mainnet VRC | Mainnet VRM | binarytest VRC | binarytest VRM |
|-------|-------------|-------------|----------------|----------------|
| Message-start magic | `70 35 22 05` | `70 35 22 05` | `44 41 43 45` | `44 41 43 45` |
| RPC port | 58683 | 33987 | 41683 | 41987 |
| P2P port | 58684 | 36988 | 41684 | 41988 |
| Pubkey base58 prefix | 70 | 70 | 25 | 50 |
| Script base58 prefix | 132 | 132 | 28 | 110 |
| Secret base58 prefix | 198 | 198 | 153 | 178 |
| Bech32 HRP | `vry` | `vry` | `vbtt` | `mbtt` |
| DNS seeds | configured | configured | none | none |
| Datadir (subdir of base) | `vericoin` | `verium` | `binarytest-vericoin` | `binarytest-verium` |

The message-start magic mismatch alone guarantees binarytest nodes cannot reach a productive handshake with mainnet peers; the additional separations are belt-and-suspenders.

## Activation

DACE activates at VRC height **50** and VRM height **50** on binarytest. The defaults in [vericoin/src/chainparams.cpp](../../src/chainparams.cpp) (`CBinaryTestVericoinParams`, `CBinaryTestVeriumParams`) set every DACE consensus constant to a fast-test profile:

| Constant | binarytest | Why |
|----------|-----------:|-----|
| BeaconDelta | 4 | Beacon every 4 VRM blocks |
| BeaconK | 4 | 4 confirmations |
| BeaconFallbackWindow | 4 | |
| BeaconEpochVRC | 10 | 10 VRC blocks per coupling epoch |
| TicketStakeUnit | 100 VRC (10^10 sat) | Easy to fund test wallets |
| TicketLockupEpochs | 3 | |
| CommitteeSize | 8 | Smaller for easy provisioning |
| StaleGraceEpochs | 2 | |
| StaleMaxEpochs | 6 | Reach recovery faster |
| RecoveryThreshold | 4/5 | Same as mainnet |
| FinalityGraceBlocks | 6 | |
| DivertSigmaBpsVRM | 400 (4%) | Same as mainnet |
| DivertPhiBpsVRC | 1000 (10%) | Same as mainnet |
| ClaimExpiryEpochs | 256 | Test claim expiry without months passing |

Block spacing is also accelerated: 5 second target on both chains.

## Selecting binarytest

CLI flags (mutually-exclusive with `-chain=`):

```
vericoind -binarytest -vericoin    # binarytest VRC
veriumd   -binarytest -verium      # binarytest VRM
```

Equivalent explicit form:

```
vericoind -chain=binarytest-vericoin
veriumd   -chain=binarytest-verium
```

## Datadirs

Inside the platform-default base directory:

- Linux: `~/.vericonomy/binarytest-vericoin/`, `~/.verium/binarytest-verium/`
- Windows: `%APPDATA%\Vericonomy\binarytest-vericoin\`, `%APPDATA%\Verium\binarytest-verium\`
- macOS: `~/Library/Application Support/Vericonomy/binarytest-vericoin/`, `~/Library/Application Support/Verium/binarytest-verium/`

Datadirs in binarytest never overlap with mainnet datadirs.

## How to use

See [vericoin/test/binarychain/README.md](../../test/binarychain/README.md) for the full test harness, including:

- Python simulation of every DACE algorithm (runs without compiling the daemon).
- Docker compose orchestration of vericoind + veriumd + observer.
- Prometheus + Grafana monitoring.
- Helper scripts for bootstrap, advance-to-activation, and teardown.

## Safety guarantees

1. **No mainnet contact.** The message-start magic differs, so binarytest nodes reject mainnet peers at the protocol-handshake level.
2. **No DNS seed pollution.** binarytest chainparams have an empty seed list. Peers are added via `-addnode` in the docker compose or manually.
3. **No address confusion.** A binarytest VRC address starts with prefix byte 25 (typically begins with `H`/`G` in base58). Mainnet VRC addresses use prefix 70 (begin with `V`). Tooling that validates address prefixes will refuse cross-network paste.
4. **No accidental promotion to mainnet.** The `binarytest` chain identifier in chainparamsbase.cpp is distinct; no codepath promotes binarytest state to mainnet.
5. **Wipeable.** All state lives in named docker volumes (`docker compose down -v` wipes everything) or in the dedicated binarytest- prefixed datadir.

## Limitations

- The C++ DACE module exists end-to-end (see [vericoin/src/dace/](../../src/dace/)) but the `dace::Service::Initialize` hookup into `init.cpp` and the `dace::AnchorLifecycle::PromoteIfActivated` hookup into `ConnectBlock` are intentionally left as one-line wires for the operator running this harness — they require a small amount of integration glue that depends on local build configuration.
- Until that wiring lands, use the Python simulator at [vericoin/test/binarychain/sim/](../../test/binarychain/sim/), which is byte-identical to the C++ algorithms and exercises every scenario in seconds.
