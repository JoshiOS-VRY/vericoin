# Using the Vericonomy Wallet on the Binarytest (DACE) Network

Status: User guide for testers
Audience: Anyone evaluating Binary Chain v3 (DACE) with the desktop wallet

The Vericonomy Wallet ([verium/desktop/verium-app/](../../../verium/desktop/verium-app/)) ships with a built-in toggle to switch between **Mainnet** (real coins) and **Binarytest** (the isolated DACE test network — play money only).

This guide walks through using the wallet for test balances, test sends, test mining (VRM), test staking (VRC), and DACE-specific features (active anchor, claim redemption, ticket registration) entirely on binarytest without touching any real funds.

## Safety contract

When the wallet is in binarytest mode:

- A persistent amber banner appears across the top of every page: **"BINARYTEST — DACE test network — funds have no real value."**
- A small `binarytest` badge appears in the sidebar footer next to the active-coin label.
- The Settings page shows the active mode with both endpoints (RPC URL + datadir) visible.
- RPC requests go to ports **41683** (VRC) and **41987** (VRM) — distinct from mainnet's 58683/33987.
- Datadirs are under **`binarytest-vericoin/`** and **`binarytest-verium/`**, completely separate from mainnet directories. **Mainnet wallet files are not touched.**
- The wallet refuses to connect to mainnet peers at the message-magic level. (See [binarytest-network.md](binarytest-network.md) for the wire-level isolation guarantees.)

## Prerequisites

1. **Build DACE-capable daemons.** The CDN sidecars that ship with the wallet do not yet include DACE / binarytest support, so you need locally-built `veriumd` and `vericoind` from the unified vericoin tree (headless daemons only — the WSL/MinGW build skips Qt):

   ```bash
   cd vericoin
   ./build-dace.sh
   ```

   On Windows (PowerShell), use WSL automatically:

   ```powershell
   .\vericoin\build-dace.ps1
   ```

   Or from the wallet directory: `npm run build:dace`

   (Legacy autotools one-liner if you prefer: `./autogen.sh` then configure/make twice.)

2. **Install the DACE sidecars into the wallet.** From the wallet directory:

   ```bash
   cd verium/desktop/verium-app
   npm run fetch:sidecars:dace
   ```

   This runs both fetch scripts with `DACE_DEV=1` and replaces any existing CDN sidecars with the monorepo build. Alternatively, set `VERIUMD_LOCAL` / `VERICOIND_LOCAL` to specific paths.

3. Install the wallet from the desktop app build at [verium/desktop/verium-app/](../../../verium/desktop/verium-app/) (or run the dev server with `npm run tauri dev`).

## Switching the wallet to binarytest

1. Launch the wallet. It defaults to mainnet — verify the sidebar shows your normal `VRM · Verium` / `VRC · Vericoin` labels with no amber badge.
2. Go to **Settings**. Scroll to the **Network** card (immediately under Wallet Backup).
3. Click **Binarytest (DACE)**.
4. A confirmation modal shows:
   - The warning text explaining the network is play money.
   - The new RPC URLs and datadirs the wallet will use after switching.
5. Click **Confirm switch**.
6. A follow-up prompt asks whether to restart the daemons now. Click **OK** — the wallet stops both daemons and starts them again with the binarytest configuration.
7. The amber **BINARYTEST** banner appears at the top of every page. You are now on the test network.

To switch back to mainnet, repeat the same flow choosing **Mainnet**. The mainnet wallet files are untouched throughout — switching is non-destructive.

## Getting test funds

### One-click funding from the Binary Chain page

The fastest path: open the **Binary Chain** page in the sidebar (visible only in binarytest mode), find the **Fund test wallet** card, and click **Fund**. This mines 10 PoW blocks to a fresh wallet address and credits coinbase rewards to the wallet. Coinbase maturity on binarytest is 4 blocks, so the balance becomes spendable a few seconds later. Use this on both Verium and Vericoin to seed each chain.

Under the hood the button calls the new `binarychain_fund_wallet` RPC, which is hard-refused on any non-binarytest network so it cannot accidentally affect mainnet.

### Test VRM via mining

VRM uses CPU mining (PoWT). With binarytest's accelerated 5-second target spacing, mining produces blocks quickly.

1. Switch the active coin to **Verium** in the sidebar.
2. Navigate to the **Mining** page.
3. The Mining page shows mining status. Click **Start mining**.
4. Within a few seconds you should see VRM blocks being mined and the wallet balance growing. (Coinbase maturity on binarytest is 4 blocks, so balances become spendable quickly.)

Behind the scenes the wallet calls `minerstart` on `veriumd` running in binarytest mode. The same RPCs that work on mainnet work here.

### Test VRC via staking

VRC uses PoST staking. To stake you need some initial coins — fund the wallet via the Binary Chain page first (see above), wait for coinbase maturity, then:

1. Switch the active coin to **Vericoin**.
2. Navigate to the **Staking** page.
3. Unlock the wallet for staking only (Vericoin's `walletpassphrase ... mintonly=true` mode).
4. Click **Start staking**.
5. With binarytest's reduced `nStakeMinAge` (60 seconds) and accelerated stake target spacing (5 seconds), staking rewards appear quickly.

## Test sends

1. **Vericoin** (Wallet page):
   - Click **Receive** to generate a binarytest VRC address. The address starts with prefix `25` (typically begins with `H`/`G` in base58) — distinct from mainnet's `V` prefix. **A binarytest address cannot be cross-pasted into a mainnet wallet; the prefix won't validate.**
   - Click **Send**, paste a second binarytest address, enter an amount, and confirm. The transaction broadcasts to the local binarytest network.
2. **Verium**: identical flow on the Verium wallet page. Binarytest VRM addresses use prefix `50` (typically begins with `M` in base58).

## Test balances

The Wallet page reads balances via standard `getwalletinfo` and `getbalances` RPCs. On binarytest these return real numbers that reflect your test mining and staking rewards.

The Dashboard page shows blended VRC + VRM balances and recent transactions, same as mainnet. The amber banner is the only persistent reminder that the numbers are play money.

## Test mining behavior

The Mining page works identically on mainnet and binarytest:

- **Threads**: configurable.
- **Hashrate**: reported by the daemon (`getmininginfo`).
- **Profitability widget**: shown but reads "binarytest" rather than real fiat numbers (the explorer-backed market data is suppressed on binarytest because there is no public binarytest explorer).
- **Auto-mine on open**: respected in both modes.

## Test staking behavior

The Staking page works identically:

- **Stake balance**, **new mint**, **stake_time** are all read via `getwalletinfo`.
- **Reward celebrations** fire on incoming stakes.
- **Auto-stake on open**: respected.

## Test DACE behavior

The **Binary Chain** page (added by the DACE work) reads:

- `binarychain_status` — activation state, current epoch, active anchor, stale-coupling indicator, bonded ticket totals.
- `binarychain_metrics` — threat-model counters.
- `binarychain_anchor` — currently activated Joint Anchor in JSON.

On binarytest the page shows the amber **BINARYTEST** chip next to the title and the live state from the binarytest daemons. Note that to see actual JA activations you need to:

1. Mine VRM past height 50 (binarytest's `BinaryChainHeightVRM`).
2. Stake VRC past height 50.
3. Register at least 8 bonded tickets (binarytest `CommitteeSize = 8`).
4. Have the committee sign anchor candidates (the Phase 2b runbook covers manual committee signing for testing).

For an automated end-to-end demonstration without setting up a committee by hand, run the [Python simulator](../../test/binarychain/sim/) — it executes the full happy-path lifecycle in seconds.

## Limitations on binarytest

- **No explorer integration.** Explorer URLs are suppressed in binarytest mode; the Network page and explorer-backed widgets show "Explorer unavailable on binarytest".
- **No bootstrap download.** Binarytest has no canonical snapshot CDN; the Bootstrap banner is suppressed.
- **Throwaway keys only.** Never paste a mainnet seed or private key into a binarytest wallet, and vice versa. The wallet's keychain entries are namespaced under `com.vericonomy.wallet.desktop.binarytest.*` to keep keychain entries from colliding, but it is still your responsibility to keep the two worlds separate.
- **Recovery from binarytest does not restore mainnet.** The two networks have separate datadirs, separate wallets, and incompatible address prefixes.

## Returning to mainnet

Switch via Settings → Network → Mainnet. Confirm. Restart the daemons when prompted.

The amber banner disappears. Your mainnet balances, addresses, and transaction history are exactly as you left them — switching modes did not touch them.
