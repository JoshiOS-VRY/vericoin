// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/sortition.h>

#include <consensus/merkle.h>
#include <hash.h>

#include <algorithm>

namespace dace {

uint256 ComputeSortitionSeed(const uint256& prev_ja_hash,
                             const uint256& beacon_hash,
                             const uint256& vrc_checkpoint_hash)
{
    CHashWriter w(SER_GETHASH, 0);
    w << prev_ja_hash;
    w << beacon_hash;
    w << vrc_checkpoint_hash;
    return w.GetHash();
}

uint256 ComputeTicketScore(const uint256& seed, const uint256& ticket_id)
{
    CHashWriter w(SER_GETHASH, 0);
    w << seed;
    w << ticket_id;
    return w.GetHash();
}

std::vector<Ticket> SampleCommittee(const std::vector<Ticket>& tickets,
                                    const uint256& seed,
                                    size_t M)
{
    if (tickets.empty() || M == 0) return {};

    // Compute (score_i, ticket) pairs, partial-sort by score ascending, then
    // sort the selected M tickets by ticket_id for deterministic downstream
    // use (committee_root, signature index assignment).
    struct ScoredTicket {
        uint256 score;
        const Ticket* ticket;
    };
    std::vector<ScoredTicket> scored;
    scored.reserve(tickets.size());
    for (const Ticket& t : tickets) {
        scored.push_back({ComputeTicketScore(seed, t.GetId()), &t});
    }

    const size_t k = std::min(M, scored.size());
    std::partial_sort(scored.begin(), scored.begin() + k, scored.end(),
                      [](const ScoredTicket& a, const ScoredTicket& b) {
                          if (a.score != b.score) return a.score < b.score;
                          // Tiebreak: lexicographic ticket_id (negligible at 256-bit).
                          return a.ticket->GetId() < b.ticket->GetId();
                      });

    std::vector<Ticket> committee;
    committee.reserve(k);
    for (size_t i = 0; i < k; ++i) {
        committee.push_back(*scored[i].ticket);
    }
    // Sort by ticket_id for downstream determinism.
    std::sort(committee.begin(), committee.end(),
              [](const Ticket& a, const Ticket& b) {
                  return a.GetId() < b.GetId();
              });
    return committee;
}

uint256 ComputeCommitteeRoot(const std::vector<Ticket>& committee)
{
    std::vector<uint256> leaves;
    leaves.reserve(committee.size());
    for (const Ticket& t : committee) {
        leaves.push_back(t.GetId());
    }
    bool mutated = false;
    return ComputeMerkleRoot(leaves, &mutated);
}

} // namespace dace
