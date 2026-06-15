// Copyright (c) 2026 The Vericonomy developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DACE_TICKET_REGISTRY_H
#define BITCOIN_DACE_TICKET_REGISTRY_H

#include <amount.h>
#include <consensus/params.h>
#include <dbwrapper.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <serialize.h>
#include <uint256.h>
#include <script/script.h>

#include <map>
#include <set>
#include <vector>

namespace dace {

/** Distinguish ticket registry transactions in coinbase / scriptSig markers. */
static constexpr uint32_t TICKET_MARKER_REGISTER = 0x44434552; // "DCER"
static constexpr uint32_t TICKET_MARKER_UNBOND   = 0x44434555; // "DCEU"

/** A bonded ticket in the registry. See DACE-3. */
struct Ticket {
    COutPoint stake_outpoint;     ///< funding output, locked for L epochs
    CPubKey   operator_pubkey;    ///< committee signing key
    uint32_t  registered_epoch;   ///< epoch the ticket became active
    uint32_t  unbond_epoch;       ///< 0 = still active; nonzero = unbond requested
    CAmount   stake_amount;       ///< must equal Consensus::Params::TicketStakeUnit

    Ticket() : registered_epoch(0), unbond_epoch(0), stake_amount(0) {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(stake_outpoint);
        READWRITE(operator_pubkey);
        READWRITE(registered_epoch);
        READWRITE(unbond_epoch);
        READWRITE(stake_amount);
    }

    /** ticket_id = SHA256d(serialize(stake_outpoint || operator_pubkey)). */
    uint256 GetId() const;

    bool IsActive(uint32_t current_epoch) const {
        return unbond_epoch == 0 || current_epoch < unbond_epoch;
    }
};

/** A claim against the registry for slashing or fund redemption. */
struct TicketSlashEvidence {
    uint256 ticket_id;
    std::vector<unsigned char> sig_a;   ///< first conflicting signature
    std::vector<unsigned char> sig_b;   ///< second conflicting signature
    uint256 ja_partial_hash_a;          ///< partial JA hash signed by sig_a
    uint256 ja_partial_hash_b;          ///< partial JA hash signed by sig_b
    uint32_t epoch_index;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ticket_id);
        READWRITE(sig_a);
        READWRITE(sig_b);
        READWRITE(ja_partial_hash_a);
        READWRITE(ja_partial_hash_b);
        READWRITE(epoch_index);
    }
};

/** Bonded ticket registry — the source of truth for the active committee
 *  candidate pool. Updated as blocks are connected; rewound on disconnect.
 *  Persisted in LevelDB at "dace/tickets/". */
class TicketRegistry {
public:
    /** Apply a register_ticket transaction. Effective at current_epoch + 1. */
    bool ApplyRegister(const CTransaction& tx, uint32_t current_epoch, std::string& err);

    /** Insert a fully-formed ticket directly into the registry. Used by RPC
     *  and dev/test code paths that build the marker transaction outside the
     *  registry. Returns false if the ticket_id is already present. */
    bool InsertDirect(const Ticket& t, std::string& err);

    /** Mark a ticket as unbonding at `unbond_at_epoch`. Returns false if the
     *  ticket is unknown or already unbonding. */
    bool BeginUnbond(const uint256& ticket_id, uint32_t unbond_at_epoch, std::string& err);

    /** Apply an unbond_ticket transaction. Effective at current_epoch + 1. */
    bool ApplyUnbond(const CTransaction& tx, uint32_t current_epoch, std::string& err);

    /** Apply slashing evidence. Burns the ticket's stake and removes it. */
    bool ApplySlash(const TicketSlashEvidence& evidence,
                    const Consensus::Params& params,
                    uint32_t current_epoch,
                    std::string& err);

    /** Snapshot of all tickets active at the start of an epoch.
     *  Sorted by ticket_id for deterministic Merkle root computation. */
    std::vector<Ticket> ActiveAtEpoch(uint32_t epoch) const;

    /** Merkle root of the active ticket set at this epoch. */
    uint256 RootAtEpoch(uint32_t epoch) const;

    /** Lookup by ticket_id. Returns nullptr if not present. */
    const Ticket* Find(const uint256& ticket_id) const;

    /** Total bonded stake across all active tickets at this epoch (sum of stake_amount). */
    CAmount TotalBondedAt(uint32_t epoch) const;

    /** Total ticket count active at this epoch. */
    size_t ActiveCountAt(uint32_t epoch) const;

    /** Disconnect support: undo last register/unbond/slash. */
    void Rewind(uint32_t to_epoch);

    /** Persist / restore (LevelDB). */
    bool Flush(CDBBatch& batch) const;
    bool Load(CDBWrapper& db);

private:
    // Keyed by ticket_id.
    std::map<uint256, Ticket> tickets_;

    // Per-epoch cached Merkle roots; lazily computed.
    mutable std::map<uint32_t, uint256> root_cache_;

    void InvalidateRoots(uint32_t from_epoch);
};

/** Decode a register_ticket transaction's marker output. Returns true if the
 *  transaction is a valid register_ticket payload (structural check only —
 *  amount/signature checks are done in ApplyRegister). */
bool DecodeRegisterTicket(const CTransaction& tx,
                          COutPoint& out_stake_outpoint,
                          CPubKey& out_operator_pubkey);

/** Decode an unbond_ticket transaction's marker output. */
bool DecodeUnbondTicket(const CTransaction& tx, uint256& out_ticket_id);

/** Helper: construct the scriptPubKey for a locked ticket stake (the script
 *  encumbers the funds against early spending). */
CScript BuildTicketLockScript(const CPubKey& operator_pubkey,
                              uint32_t registered_epoch,
                              int lockup_epochs);

} // namespace dace

#endif // BITCOIN_DACE_TICKET_REGISTRY_H
