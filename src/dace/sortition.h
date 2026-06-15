// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_SORTITION_H
#define BITCOIN_DACE_SORTITION_H

#include <dace/ticket_registry.h>
#include <uint256.h>

#include <vector>

namespace dace {

/** Compute the sortition seed per DACE-3:
 *  S_e = H( prev_ja_hash || beacon_hash || vrc_checkpoint_hash ) */
uint256 ComputeSortitionSeed(const uint256& prev_ja_hash,
                             const uint256& beacon_hash,
                             const uint256& vrc_checkpoint_hash);

/** Per-ticket score: score_i = H( seed || ticket_id ). */
uint256 ComputeTicketScore(const uint256& seed, const uint256& ticket_id);

/** Select the M lowest-score tickets from `tickets`, without replacement.
 *  Ties broken by lexicographic order of ticket_id (statistically negligible
 *  at 256-bit scores). Returns a vector of length min(M, tickets.size())
 *  sorted by ticket_id for deterministic downstream use (committee_root). */
std::vector<Ticket> SampleCommittee(const std::vector<Ticket>& tickets,
                                    const uint256& seed,
                                    size_t M);

/** Compute the committee Merkle root from a committee returned by
 *  SampleCommittee (already sorted by ticket_id). */
uint256 ComputeCommitteeRoot(const std::vector<Ticket>& committee);

} // namespace dace

#endif // BITCOIN_DACE_SORTITION_H
