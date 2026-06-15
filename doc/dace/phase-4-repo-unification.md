# Phase 4 — Repo Unification Plan

Status: Draft
Phase: 4 (post-mainnet hardening)
Prerequisite: Phase 3b mainnet success ≥ 90 days, public retrospective published.

## Objective

Deprecate the standalone [verium/](../../../verium/) repository for new consensus development. All consensus rules continue to live in the unified [vericoin/src/](../../src/) tree, which already supports both daemons via the `CLIENT_IS_VERIUM` build flag.

## Background

Historically, Verium has been maintained as a separate fork at [verium/](../../../verium/). The unified [vericoin/src/](../../src/) tree already contains both `CVericoinParams` and `CVeriumParams` and builds either `veriumd` or `vericoind` selected at configure time by `CLIENT_IS_VERIUM`. With DACE live on mainnet, maintaining two implementations of cross-chain consensus rules is unnecessary risk and effort.

## Migration plan

### Step 1: Freeze the standalone verium repo for new consensus work (T+90)

- The standalone [verium/](../../../verium/) repo enters maintenance mode.
- Only security backports from the unified tree are applied.
- New protocol work (any future DACE-9, DACE-10, ...) happens exclusively in the unified tree.

### Step 2: Cut a final standalone verium release (T+120)

- Tagged release of the standalone fork containing the final consensus parity with the unified tree.
- This release is what running standalone VRM operators upgrade to before the deprecation.

### Step 3: Migration window for VRM operators (T+120 to T+180)

- Operators are encouraged to switch from the standalone `verium` build to `veriumd` produced by the unified `vericoin` tree (`./configure --enable-verium && make`).
- Both binaries are consensus-compatible (the unified tree is the source of truth for the standalone tree's final release).

### Step 4: Standalone repo archival (T+180)

- The standalone [verium/](../../../verium/) repository is marked archived on GitHub.
- README updated to point at the unified [vericoin/](../../) repo.
- Issues and PRs locked.

### Step 5: Desktop wallet update (T+180)

- [verium/desktop/verium-app/](../../../verium/desktop/verium-app/) lives at the same path but its sidecar download paths in [src-tauri/src/coin_profile.rs](../../../verium/desktop/verium-app/src-tauri/src/coin_profile.rs) point to the unified-built binaries.
- Build CI ([fetch-veriumd.cjs](../../../verium/desktop/verium-app/scripts/fetch-veriumd.cjs)) is updated to fetch from the unified release stream.

## Validation

Before each step, verify:

| Check | How |
|-------|-----|
| Both daemons compile from unified tree | `./configure --enable-verium && make && ./configure --enable-vericoin && make` |
| Both daemons produce bit-identical block hashes vs the standalone tree for at least the last 10,000 mainnet blocks | Replay-from-block test harness |
| The unified `veriumd` participates correctly in the live mainnet | 7-day node observation |
| Hash function fingerprints match exactly | Compare benchmark outputs across builds |

## Risk

The standalone tree's deprecation is a coordination event for VRM operators, but not a consensus event. There is no flag day; operators migrate at their own pace within the migration window. The unified `veriumd` is functionally identical to the standalone build.

If a meaningful subset of operators (≥ 10% of hashpower) cannot migrate within the window, the window is extended.

## Long-term

After Step 5, all consensus work for both chains lives in one tree. This:

- Eliminates parallel maintenance of cross-chain logic.
- Simplifies future protocol upgrades (one PR, two daemons).
- Reduces audit surface (one codebase, two configurations).

The unified tree's `CLIENT_IS_VERIUM` build flag remains the long-term mechanism for distinguishing the two daemons.
