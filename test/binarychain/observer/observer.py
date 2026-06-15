"""Binary Chain v3 (DACE) observer.

Polls binarychain_status and binarychain_metrics on both daemons and
exposes them on http://0.0.0.0:9101/metrics for Prometheus scraping.
"""

from __future__ import annotations

import json
import os
import time
from typing import Any, Dict

import requests
from prometheus_client import Gauge, start_http_server


VRC_URL = os.environ.get("VRC_RPC_URL", "http://rpcuser:rpcpass@127.0.0.1:41683")
VRM_URL = os.environ.get("VRM_RPC_URL", "http://rpcuser:rpcpass@127.0.0.1:41987")
INTERVAL = int(os.environ.get("OBSERVER_INTERVAL_SEC", "10"))


# ---------------------------------------------------------------------------
# Prometheus gauges. Labels distinguish the two chain sides.
# ---------------------------------------------------------------------------

g_activated = Gauge("bc_activated", "DACE active on this chain", ["chain"])
g_epoch = Gauge("bc_current_epoch", "Current coupling epoch", ["chain"])
g_stale = Gauge("bc_stale_coupled", "Stale-coupling indicator", ["chain"])
g_tickets = Gauge("bc_bonded_tickets_active", "Active bonded tickets", ["chain"])
g_bonded_amount = Gauge("bc_bonded_amount_satoshi", "Total bonded amount (satoshi)", ["chain"])
g_lag_p50 = Gauge("bc_paired_header_lag_p50_sec", "Paired header lag p50 (s)", ["chain"])
g_lag_p95 = Gauge("bc_paired_header_lag_p95_sec", "Paired header lag p95 (s)", ["chain"])
g_lag_p99 = Gauge("bc_paired_header_lag_p99_sec", "Paired header lag p99 (s)", ["chain"])

g_divergent_beacons = Gauge("bc_divergent_beacon_selections", "Divergent beacon selections", ["chain"])
g_reorgs_at_beacon = Gauge("bc_reorgs_at_beacon", "Reorgs at beacon", ["chain"])
g_missed_votes = Gauge("bc_committee_missed_votes", "Committee missed votes", ["chain"])
g_foreign_rejects = Gauge("bc_foreign_payee_rejections", "Foreign payee rejections", ["chain"])
g_missed_anchors = Gauge("bc_consecutive_missed_anchors", "Consecutive missed anchors", ["chain"])
g_ibd_diffs = Gauge("bc_ibd_replay_consensus_diffs", "IBD replay consensus diffs", ["chain"])
g_stale_entries = Gauge("bc_stale_coupling_entries", "Stale-coupling entries", ["chain"])
g_recovery_built = Gauge("bc_recovery_anchors_built", "Recovery anchors built", ["chain"])
g_recovery_active = Gauge("bc_recovery_anchors_activated", "Recovery anchors activated", ["chain"])
g_claims_total = Gauge("bc_claim_redemptions_total", "Claim redemptions total", ["chain"])
g_claims_rejected = Gauge("bc_claim_redemptions_rejected", "Claim redemptions rejected", ["chain"])
g_slashes = Gauge("bc_slashes_total", "Slashes total", ["chain"])
g_top10_share = Gauge("bc_top10_ticket_share", "Top-10 ticket share", ["chain"])
g_seat_ratio = Gauge("bc_seat_to_bonded_ratio_top10", "Seat/bonded ratio top-10", ["chain"])


def rpc_call(url: str, method: str, params: list | None = None) -> Any:
    body = {"jsonrpc": "1.0", "id": "observer", "method": method, "params": params or []}
    r = requests.post(url, data=json.dumps(body), headers={"Content-Type": "application/json"},
                      timeout=8)
    r.raise_for_status()
    return r.json().get("result")


def poll_chain(label: str, url: str) -> None:
    try:
        status: Dict[str, Any] = rpc_call(url, "binarychain_status") or {}
        metrics: Dict[str, Any] = rpc_call(url, "binarychain_metrics") or {}
    except Exception as e:
        print(f"[{label}] poll failed: {e}")
        return

    g_activated.labels(chain=label).set(1 if status.get("activated") else 0)
    g_epoch.labels(chain=label).set(status.get("current_epoch") or 0)
    g_stale.labels(chain=label).set(1 if status.get("stale_coupled") else 0)
    g_tickets.labels(chain=label).set(status.get("bonded_tickets_active") or 0)
    g_bonded_amount.labels(chain=label).set(status.get("bonded_tickets_total_amount") or 0)
    g_lag_p50.labels(chain=label).set(status.get("paired_header_lag_p50_sec") or 0)
    g_lag_p95.labels(chain=label).set(status.get("paired_header_lag_p95_sec") or 0)
    g_lag_p99.labels(chain=label).set(status.get("paired_header_lag_p99_sec") or 0)

    g_divergent_beacons.labels(chain=label).set(metrics.get("divergent_beacon_selections") or 0)
    g_reorgs_at_beacon.labels(chain=label).set(metrics.get("reorgs_at_beacon") or 0)
    g_missed_votes.labels(chain=label).set(metrics.get("committee_missed_votes") or 0)
    g_foreign_rejects.labels(chain=label).set(metrics.get("foreign_payee_rejections") or 0)
    g_missed_anchors.labels(chain=label).set(metrics.get("consecutive_missed_anchors") or 0)
    g_ibd_diffs.labels(chain=label).set(metrics.get("ibd_replay_consensus_diffs") or 0)
    g_stale_entries.labels(chain=label).set(metrics.get("stale_coupling_entries") or 0)
    g_recovery_built.labels(chain=label).set(metrics.get("recovery_anchors_built") or 0)
    g_recovery_active.labels(chain=label).set(metrics.get("recovery_anchors_activated") or 0)
    g_claims_total.labels(chain=label).set(metrics.get("claim_redemptions_total") or 0)
    g_claims_rejected.labels(chain=label).set(metrics.get("claim_redemptions_rejected") or 0)
    g_slashes.labels(chain=label).set(metrics.get("slashes_total") or 0)
    g_top10_share.labels(chain=label).set(metrics.get("top10_ticket_share") or 0)
    g_seat_ratio.labels(chain=label).set(metrics.get("seat_to_bonded_ratio_top10") or 1.0)


def main() -> None:
    print(f"BC observer up on :9101 (interval {INTERVAL}s)")
    start_http_server(9101)
    while True:
        poll_chain("vrc", VRC_URL)
        poll_chain("vrm", VRM_URL)
        time.sleep(INTERVAL)


if __name__ == "__main__":
    main()
