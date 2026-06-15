// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dace/ticket_registry.h>

#include <consensus/merkle.h>
#include <consensus/params.h>
#include <dbwrapper.h>
#include <hash.h>
#include <script/script.h>
#include <script/standard.h>
#include <streams.h>
#include <util/strencodings.h>

#include <algorithm>

namespace dace {

uint256 Ticket::GetId() const
{
    CHashWriter w(SER_GETHASH, 0);
    w << stake_outpoint;
    w << operator_pubkey;
    return w.GetHash();
}

namespace {

// Marker output detection: a register/unbond transaction places a zero-value
// output whose scriptPubKey starts with OP_RETURN, a 4-byte marker, and the
// payload (CPubKey for register, ticket_id for unbond).
//
// We avoid script template parsing complexity by using a fixed scriptPubKey
// shape: OP_RETURN <push 4 bytes marker> <push N bytes payload>.

bool ExtractMarkerOutput(const CTransaction& tx,
                         uint32_t expected_marker,
                         std::vector<unsigned char>& out_payload)
{
    for (const CTxOut& vout : tx.vout) {
        if (vout.nValue != 0) continue;
        const CScript& script = vout.scriptPubKey;
        CScript::const_iterator it = script.begin();

        opcodetype op;
        std::vector<unsigned char> push;

        if (!script.GetOp(it, op) || op != OP_RETURN) continue;
        if (!script.GetOp(it, op, push) || push.size() != 4) continue;
        uint32_t marker = ReadLE32(push.data());
        if (marker != expected_marker) continue;

        if (!script.GetOp(it, op, out_payload)) continue;
        return true;
    }
    return false;
}

} // namespace

bool DecodeRegisterTicket(const CTransaction& tx,
                          COutPoint& out_stake_outpoint,
                          CPubKey& out_operator_pubkey)
{
    std::vector<unsigned char> payload;
    if (!ExtractMarkerOutput(tx, TICKET_MARKER_REGISTER, payload)) return false;

    // Payload: serialized COutPoint || serialized CPubKey
    try {
        CDataStream ss(payload, SER_NETWORK, PROTOCOL_VERSION);
        ss >> out_stake_outpoint;
        ss >> out_operator_pubkey;
    } catch (...) {
        return false;
    }
    return out_operator_pubkey.IsFullyValid();
}

bool DecodeUnbondTicket(const CTransaction& tx, uint256& out_ticket_id)
{
    std::vector<unsigned char> payload;
    if (!ExtractMarkerOutput(tx, TICKET_MARKER_UNBOND, payload)) return false;
    if (payload.size() != 32) return false;
    memcpy(out_ticket_id.begin(), payload.data(), 32);
    return true;
}

CScript BuildTicketLockScript(const CPubKey& operator_pubkey,
                              uint32_t /*registered_epoch*/,
                              int /*lockup_epochs*/)
{
    // For Phase 1 the lock script is simply P2PKH against the operator's
    // pubkey, with the encumbrance enforced by the unbond_ticket
    // transaction acceptance rule and consensus-level mempool / wallet
    // refusal to spend the locked outpoint before unbond completes.
    //
    // A future revision (post-mainnet) may upgrade to a CSV-locked script
    // that enforces the timelock at the script layer too; this requires a
    // separate proposal because BIP112 semantics on VRC's PoST chain need
    // their own evaluation.
    return CScript() << OP_DUP << OP_HASH160
                     << ToByteVector(operator_pubkey.GetID())
                     << OP_EQUALVERIFY << OP_CHECKSIG;
}

bool TicketRegistry::ApplyRegister(const CTransaction& tx,
                                   uint32_t current_epoch,
                                   std::string& err)
{
    COutPoint stake_outpoint;
    CPubKey operator_pubkey;
    if (!DecodeRegisterTicket(tx, stake_outpoint, operator_pubkey)) {
        err = "register_ticket: malformed marker output";
        return false;
    }

    // Stake unit amount check: find the first non-marker output and require
    // it to equal Consensus::Params::TicketStakeUnit. This output is the
    // funding output for the ticket.
    CAmount stake_amount = 0;
    bool stake_found = false;
    for (const CTxOut& vout : tx.vout) {
        if (vout.nValue == 0) continue; // skip marker outputs
        if (!stake_found) {
            stake_amount = vout.nValue;
            stake_found = true;
        }
    }
    if (!stake_found) {
        err = "register_ticket: missing stake output";
        return false;
    }
    // Amount validation against Consensus::Params::TicketStakeUnit happens in
    // the validator that calls this method (it has access to params); we just
    // record what we saw.

    Ticket t;
    t.stake_outpoint = stake_outpoint;
    t.operator_pubkey = operator_pubkey;
    t.registered_epoch = current_epoch + 1;
    t.unbond_epoch = 0;
    t.stake_amount = stake_amount;

    uint256 id = t.GetId();
    if (tickets_.find(id) != tickets_.end()) {
        err = "register_ticket: duplicate ticket_id";
        return false;
    }
    tickets_[id] = t;
    InvalidateRoots(t.registered_epoch);
    return true;
}

bool TicketRegistry::InsertDirect(const Ticket& t, std::string& err)
{
    uint256 id = t.GetId();
    if (tickets_.find(id) != tickets_.end()) {
        err = "register_ticket: duplicate ticket_id";
        return false;
    }
    tickets_[id] = t;
    InvalidateRoots(t.registered_epoch);
    return true;
}

bool TicketRegistry::BeginUnbond(const uint256& ticket_id,
                                 uint32_t unbond_at_epoch,
                                 std::string& err)
{
    auto it = tickets_.find(ticket_id);
    if (it == tickets_.end()) {
        err = "unbond_ticket: unknown ticket_id";
        return false;
    }
    if (it->second.unbond_epoch != 0) {
        err = "unbond_ticket: already unbonding";
        return false;
    }
    it->second.unbond_epoch = unbond_at_epoch;
    InvalidateRoots(unbond_at_epoch);
    return true;
}

bool TicketRegistry::ApplyUnbond(const CTransaction& tx,
                                 uint32_t current_epoch,
                                 std::string& err)
{
    uint256 id;
    if (!DecodeUnbondTicket(tx, id)) {
        err = "unbond_ticket: malformed marker output";
        return false;
    }
    auto it = tickets_.find(id);
    if (it == tickets_.end()) {
        err = "unbond_ticket: unknown ticket_id";
        return false;
    }
    if (it->second.unbond_epoch != 0) {
        err = "unbond_ticket: already unbonding";
        return false;
    }
    it->second.unbond_epoch = current_epoch + 1;
    InvalidateRoots(it->second.unbond_epoch);
    return true;
}

bool TicketRegistry::ApplySlash(const TicketSlashEvidence& evidence,
                                const Consensus::Params& /*params*/,
                                uint32_t current_epoch,
                                std::string& err)
{
    auto it = tickets_.find(evidence.ticket_id);
    if (it == tickets_.end()) {
        err = "slash: unknown ticket_id";
        return false;
    }
    // Conflicting signatures verification happens at the validator layer; here
    // we trust that the caller already verified both sig_a and sig_b against
    // operator_pubkey over the conflicting JA hashes for evidence.epoch_index.
    tickets_.erase(it);
    InvalidateRoots(current_epoch);
    return true;
}

std::vector<Ticket> TicketRegistry::ActiveAtEpoch(uint32_t epoch) const
{
    std::vector<Ticket> active;
    active.reserve(tickets_.size());
    for (const auto& kv : tickets_) {
        const Ticket& t = kv.second;
        if (t.registered_epoch <= epoch && t.IsActive(epoch)) {
            active.push_back(t);
        }
    }
    // Sort by ticket_id for deterministic ordering.
    std::sort(active.begin(), active.end(),
              [](const Ticket& a, const Ticket& b) {
                  return a.GetId() < b.GetId();
              });
    return active;
}

uint256 TicketRegistry::RootAtEpoch(uint32_t epoch) const
{
    auto cached = root_cache_.find(epoch);
    if (cached != root_cache_.end()) return cached->second;

    std::vector<Ticket> active = ActiveAtEpoch(epoch);
    std::vector<uint256> leaves;
    leaves.reserve(active.size());
    for (const Ticket& t : active) {
        leaves.push_back(t.GetId());
    }
    bool mutated = false;
    uint256 root = ComputeMerkleRoot(leaves, &mutated);
    if (!mutated) {
        root_cache_[epoch] = root;
    }
    return root;
}

const Ticket* TicketRegistry::Find(const uint256& ticket_id) const
{
    auto it = tickets_.find(ticket_id);
    if (it == tickets_.end()) return nullptr;
    return &it->second;
}

CAmount TicketRegistry::TotalBondedAt(uint32_t epoch) const
{
    CAmount total = 0;
    for (const Ticket& t : ActiveAtEpoch(epoch)) {
        total += t.stake_amount;
    }
    return total;
}

size_t TicketRegistry::ActiveCountAt(uint32_t epoch) const
{
    return ActiveAtEpoch(epoch).size();
}

void TicketRegistry::InvalidateRoots(uint32_t from_epoch)
{
    auto it = root_cache_.lower_bound(from_epoch);
    root_cache_.erase(it, root_cache_.end());
}

void TicketRegistry::Rewind(uint32_t to_epoch)
{
    // Drop tickets registered after to_epoch; restore unbond_epoch == 0 for
    // tickets that were marked for unbond after to_epoch.
    for (auto it = tickets_.begin(); it != tickets_.end();) {
        if (it->second.registered_epoch > to_epoch + 1) {
            it = tickets_.erase(it);
            continue;
        }
        if (it->second.unbond_epoch != 0 && it->second.unbond_epoch > to_epoch + 1) {
            it->second.unbond_epoch = 0;
        }
        ++it;
    }
    InvalidateRoots(to_epoch);
}

namespace {
constexpr char DB_TICKET_PREFIX = 'T';
} // namespace

bool TicketRegistry::Flush(CDBBatch& batch) const
{
    // Note: callers are expected to clear prefix entries before reflushing;
    // for incremental flush, change set bookkeeping is added in Phase 2.
    for (const auto& kv : tickets_) {
        batch.Write(std::make_pair(DB_TICKET_PREFIX, kv.first), kv.second);
    }
    return true;
}

bool TicketRegistry::Load(CDBWrapper& db)
{
    tickets_.clear();
    root_cache_.clear();
    std::unique_ptr<CDBIterator> it(db.NewIterator());
    for (it->Seek(std::make_pair(DB_TICKET_PREFIX, uint256())); it->Valid(); it->Next()) {
        std::pair<char, uint256> key;
        if (!it->GetKey(key) || key.first != DB_TICKET_PREFIX) break;
        Ticket t;
        if (!it->GetValue(t)) continue;
        tickets_[key.second] = t;
    }
    return true;
}

} // namespace dace
