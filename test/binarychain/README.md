# Binary Chain v3 (DACE) — Test Harness

Everything you need to exercise DACE end-to-end without touching mainnet.

## Safety

This harness is **physically isolated** from mainnet at four layers:

| Layer | Mainnet | binarytest |
|-------|---------|------------|
| Message-start magic | `0x70 0x35 0x22 0x05` | `0x44 0x41 0x43 0x45` ("DACE") |
| VRC RPC / P2P ports | 58683 / 58684 | 41683 / 41684 |
| VRM RPC / P2P ports | 33987 / 36988 | 41987 / 41988 |
| Base58 address prefix (VRC) | 70 | 25 |
| Bech32 HRP (VRC) | `vry` | `vbtt` |
| DNS seeds | configured | **none** |
| Datadir | `vericoin/` / `verium/` | `binarytest-vericoin/` / `binarytest-verium/` |

A binarytest daemon physically cannot peer with a mainnet node because the
message-start magic differs at the wire level. Addresses and keys generated
in binarytest cannot be cross-pasted into a mainnet wallet because the
base58 prefixes differ.

The chain is selected with `-binarytest` plus either `-vericoin` or `-verium`.
See [vericoin/src/chainparams.cpp](../../src/chainparams.cpp)
`CBinaryTestVericoinParams` and `CBinaryTestVeriumParams`.

## Two tracks

This harness supports two ways to exercise DACE depending on how far the
C++ daemon wiring has progressed:

### Track A — Python simulation (works today)

A self-contained Python implementation of every DACE algorithm — matching the
C++ reference at [vericoin/src/dace/](../../src/dace/) byte-for-byte — runs
every scenario in seconds without compiling the C++ daemon.

```bash
# From the repo root
python vericoin/test/binarychain/sim/scenarios/run_all.py
```

Each scenario is also runnable standalone:

| Script | What it demonstrates |
|--------|----------------------|
| `sim/scenarios/happy_path.py` | Full observe → certify → activate lifecycle of a Joint Anchor, with reward accumulator and committee sortition. |
| `sim/scenarios/beacon_reorg.py` | Height-based beacon stays deterministic across a VRM reorg; fallback ladder advances when needed. |
| `sim/scenarios/sortition_splitting.py` | Without-replacement committee sampling caps a Sybil splitter's influence at linear share, not superlinear. |
| `sim/scenarios/stale_coupling.py` | Paired-data outage triggers stale-coupling, reaches RecoveryEligible, then resumes cleanly. |
| `sim/scenarios/recovery_anchor.py` | 80% bonded supermajority assembles a recovery anchor that resumes finality after a stall. |
| `sim/scenarios/claim_redemption.py` | Pull-based Merkle claim proof: valid claim accepted, tampered proof rejected, replay rejected. |
| `sim/scenarios/equivocation_slash.py` | Two distinct anchors for the same epoch produce slashable evidence. |

### Track B — Isolated dockerized network

`dace::Service` is wired into `init.cpp`, validation, mining/staking, and the
P2P layer, so the docker compose orchestration spins up a real dual-daemon
binarytest network that exercises end-to-end DACE behavior:

```bash
# One-command acceptance: bring up both daemons, advance past activation,
# and assert binarychain_status.activated == true on both sides.
bash vericoin/test/binarychain/scripts/binarytest-demo.sh

# Or run the steps individually:
vericoin/test/binarychain/scripts/bootstrap.sh
vericoin/test/binarychain/scripts/advance-to-activation.sh
vericoin/test/binarychain/scripts/observe.sh

# Optional: bring up Prometheus + Grafana monitoring
docker compose \
    -f vericoin/test/binarychain/docker/docker-compose.yml \
    -f vericoin/test/binarychain/monitoring/docker-compose.monitoring.yml \
    up -d

# Grafana at http://localhost:3000 (anonymous read access).
# Dashboard: "Binary Chain (binarytest)".

# Teardown — wipes all volumes
vericoin/test/binarychain/scripts/teardown.sh
```

Windows users have `.ps1` equivalents for `bootstrap` and `teardown`.

## Architecture

```mermaid
flowchart TB
    subgraph host [Your laptop]
        direction TB
        subgraph docker [docker network bc-binarytest 172.28.0.0/16]
            VRC[vericoind<br/>172.28.0.10<br/>RPC 41683 / P2P 41684]
            VRM[veriumd<br/>172.28.0.20<br/>RPC 41987 / P2P 41988]
            OBS[observer<br/>172.28.0.30<br/>:9101 /metrics]
            VRC <-->|getxheaders / xheaders| VRM
            OBS -->|RPC poll| VRC
            OBS -->|RPC poll| VRM
        end
        subgraph monitoring [optional]
            PROM[Prometheus :9090]
            GRAF[Grafana :3000]
            PROM --> OBS
            GRAF --> PROM
        end
        WALLET[Vericonomy Wallet<br/>desktop]
        WALLET -->|RPC| VRC
        WALLET -->|RPC| VRM
    end
    subgraph sim [Track A]
        SIM[Python DACE simulator<br/>vericoin/test/binarychain/sim/]
    end
```

## Constants (binarytest profile)

Defaults are tuned for fast tests. See [DACE-0](../../doc/dace/DACE-0-overview-and-constants.md)
for the mainnet Balanced profile.

| Constant | Mainnet (Balanced) | binarytest | Why |
|----------|--------------------|------------|------|
| `BinaryChainHeightVRC` / `VRM` | TBD at activation | 50 | Reach activation in seconds |
| `BeaconDelta` | 12 | 4 | Smaller stride |
| `BeaconK` | 50 | 4 | Faster epochs |
| `BeaconEpochVRC` | 60 blocks | 10 blocks | Faster epochs |
| `TicketStakeUnit` | 1000 VRC | 100 VRC | Easier to fund test wallets |
| `TicketLockupEpochs` | 6 | 3 | Test unbond faster |
| `CommitteeSize` | 128 | 8 | Easier to provision |
| `StaleMaxEpochs` | 16 | 6 | Reach recovery faster |

Modify [`chainparams.cpp`](../../src/chainparams.cpp) `CBinaryTestVericoinParams` /
`CBinaryTestVeriumParams` to retune. (Changes apply on next daemon restart.)

## Wallet integration

The desktop wallet at [verium/desktop/verium-app/](../../../verium/desktop/verium-app/)
already exposes `binarychain_status`, `binarychain_metrics`, `binarychain_anchor`
RPCs via the [DACE Tauri commands](../../../verium/desktop/verium-app/src-tauri/src/dace_commands.rs)
and the [BinaryChain page](../../../verium/desktop/verium-app/src/pages/BinaryChain.tsx).

To point the wallet at the binarytest network, override the RPC port in the
daemon config it loads:

```jsonc
// daemon-vericoin.json
{
  "rpc_url": "http://rpcuser:rpcpass@127.0.0.1:41683",
  "chain": "binarytest-vericoin"
}
```

Same for verium. With these in place, the wallet's Binary Chain page reads
live data from the dockerized binarytest daemons.

## File map

```
vericoin/test/binarychain/
├── README.md                       # this file
├── sim/
│   ├── __init__.py
│   ├── dace_sim.py                 # all DACE algorithms in Python
│   └── scenarios/
│       ├── _common.py
│       ├── happy_path.py
│       ├── beacon_reorg.py
│       ├── sortition_splitting.py
│       ├── stale_coupling.py
│       ├── recovery_anchor.py
│       ├── claim_redemption.py
│       ├── equivocation_slash.py
│       └── run_all.py
├── docker/
│   ├── docker-compose.yml          # vericoind + veriumd + observer
│   ├── Dockerfile.vericoind
│   ├── Dockerfile.veriumd
│   ├── Dockerfile.observer
│   └── conf/
│       ├── vericoin-binarytest.conf
│       └── verium-binarytest.conf
├── observer/
│   └── observer.py                 # exposes Prometheus /metrics
├── monitoring/
│   ├── docker-compose.monitoring.yml
│   ├── prometheus.yml
│   └── grafana/
│       ├── provisioning/
│       │   ├── datasources/prometheus.yml
│       │   └── dashboards/binarychain.yml
│       └── dashboards/binarychain.json
└── scripts/
    ├── bootstrap.sh / bootstrap.ps1
    ├── advance-to-activation.sh
    ├── observe.sh
    └── teardown.sh / teardown.ps1
```

## What this harness intentionally does **not** include

- Mainnet seed peers or mainnet-port forwarding rules. binarytest cannot
  reach mainnet.
- Any "real money" key material. All keys in this harness are throwaway test
  keys.
- A faucet or block explorer for the binarytest chain. (Easy to add: see
  [vericonomyexplorer-v2/](../../../vericonomyexplorer-v2/) which already
  supports custom RPC endpoints — point it at port 41683.)

## Where to file issues

If a scenario diverges from the C++ implementation, or if you reproduce a
DACE bug in the harness:

1. Capture the seed / scenario name.
2. Save the output of `binarychain_status` + `binarychain_metrics` from both
   daemons (or the Python sim's state dump).
3. Open an issue with `dace:` prefix in the title.

See also [Phase 2c audit charter](../../doc/dace/phase-2c-audit-charter.md)
for the formal review process this harness supports.
