#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include "../internal/defines.hpp"

using namespace eosio;
using namespace std;

class [[eosio::contract("rescmng.xsat")]] resource_management : public contract {
   public:
    using contract::contract;

    /**
     * ## STRUCT `CheckResult`
     *
     * ### params
     *
     * - `{bool} has_auth` - the client's permission correct?
     * - `{bool} is_exists` - does the client account exist?
     * - `{asset} balance` - client balance
     *
     * ### example
     *
     * ```json
     * {
     *   "has_auth": true,
     *   "is_exists": true,
     *   "balance": "0.99999219 BTC"
     * }
     * ```
     */
    struct CheckResult {
        bool has_auth;
        bool is_exists;
        asset balance;
    };

    /**
     * ## TABLE `config`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{name} fee_account` - account number for receiving handling fees
     * - `{bool} disabled_withdraw` - whether withdrawal of balance is allowed
     * - `{asset} cost_per_slot` - cost per slot
     * - `{asset} cost_per_upload` - cost per upload chunk
     * - `{asset} cost_per_verification` - the cost of each verification performed
     * - `{asset} cost_per_endorsement` - the cost of each execution of an endorsement
     * - `{asset} cost_per_parse` - cost per execution of parsing
     *
     * ### example
     *
     * ```json
     * {
     *   "fee_account": "fees.xsat",
     *   "disabled_withdraw": 0,
     *   "cost_per_slot": "0.00000001 BTC",
     *   "cost_per_upload": "0.00000001 BTC",
     *   "cost_per_verification": "0.00000001 BTC",
     *   "cost_per_endorsement": "0.00000001 BTC",
     *   "cost_per_parse": "0.00000001 BTC"
     * }
     * ```
     */
    struct [[eosio::table]] config_row {
        name fee_account;
        bool disabled_withdraw = false;
        asset cost_per_slot = {0, BTC_SYMBOL};
        asset cost_per_upload = {0, BTC_SYMBOL};
        asset cost_per_verification = {0, BTC_SYMBOL};
        asset cost_per_endorsement = {0, BTC_SYMBOL};
        asset cost_per_parse = {0, BTC_SYMBOL};
    };
    typedef eosio::singleton<"config"_n, config_row> config_table;

    /**
     * ## TABLE `accounts`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{name} owner` - recharge account
     * - `{asset} balance` - account balance
     *
     * ### example
     *
     * ```json
     * {
     *   "owner": "test.xsat",
     *   "balance": "0.99999765 BTC"
     * }
     * ```
     */
    struct [[eosio::table]] account_row {
        name owner;
        asset balance;
        uint64_t primary_key() const { return owner.value; }
    };
    typedef eosio::multi_index<"accounts"_n, account_row> account_table;

    /**
     * ## TABLE `heartbeats`
     *
     * ### scope `get_self()`
     * ### params
     * - `{name} client` - client account
     * - `{uint8_t} type` - client type 1: synchronizer 2: validator
     * - `{string} version` - client version
     * - `{time_point_sec} last_heartbeat` - last heartbeat time
     *
     * ### example
     * ```json
     * {
     *  "client": "alice",
     *  "type": 1,
     *  "version": "v1.0.0",
     *  "last_heartbeat": "2024-08-13T00:00:00",
     * }
     * ```
     */
    struct [[eosio::table]] heartbeat_row {
        name client;
        uint8_t type;
        string version;
        time_point_sec last_heartbeat;
        uint64_t primary_key() const { return client.value; }
    };
    typedef eosio::multi_index<"heartbeats"_n, heartbeat_row> heartbeat_table;

    /**
     * ## TABLE `feestats`
     * 
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} height` - block height
     * - `{asset} blkent_fee` - fee from blkent.xsat contract
     * - `{asset} blksync_fee` - fee from blksync.xsat contract 
     * - `{asset} utxomng_fee` - fee from utxomng.xsat contract
     * - `{asset} total_fee` - total fee amount
     *
     * ### example
     *
     * ```json
     * {
     *   "height": 840000,
     *   "blkent_fee": "0.00000100 BTC",
     *   "blksync_fee": "0.00000200 BTC", 
     *   "utxomng_fee": "0.00000300 BTC",
     *   "total_fee": "0.00000600 BTC"
     * }
     * ```
     */
    struct [[eosio::table]] feestat_row {
        uint64_t height;
        asset blkent_fee;
        asset blksync_fee; 
        asset utxomng_fee;
        asset total_fee;

        uint64_t primary_key() const { return height; }
    };
    typedef eosio::multi_index<"feestats"_n, feestat_row> feestat_table;

    /**
     * ## ACTION `checkclient`
     *
     * - **authority**: `anyone`
     *
     * > Verify that the client is ready.
     *
     * ### params
     *
     * - `{name} client` - client account
     * - `{uint8_t} type` - client type 1: synchronizer 2: validator
     * - `{optional<string>} version` - client version
     *
     * ### example
     *
     * ```bash
     * $ cleos push action rescmng.xsat checkclient '["alice", 1, "v1.0.0"]' -p alice
     * ```
     */
    [[eosio::action]]
    CheckResult checkclient(const name& client, const uint8_t type, const optional<string>& version);

    /**
     * ## ACTION `init`
     *
     * - **authority**: `get_self()`
     *
     * > Modify or update fee configuration.
     *
     * ### params
     *
     * - `{name} fee_account` - account number for receiving handling fees
     * - `{asset} cost_per_slot` - cost per slot
     * - `{asset} cost_per_upload` - cost per upload chunk
     * - `{asset} cost_per_verification` - the cost of each verification performed
     * - `{asset} cost_per_endorsement` - the cost of each execution of an endorsement
     * - `{asset} cost_per_parse` - cost per execution of parsing
     *
     * ### example
     *
     * ```bash
     * $ cleos push action rescmng.xsat init '["fee.xsat", "0.00000001 BTC",, "0.00000001 BTC", "0.00000001 BTC", "0.00000001 BTC", "0.00000001 BTC"]' -p rescmng.xsat
     * ```
     */
    [[eosio::action]]
    void init(const name& fee_account, const asset& cost_per_slot, const asset& cost_per_upload,
              const asset& cost_per_verification, const asset& cost_per_endorsement, const asset& cost_per_parse);

    /**
     * ## ACTION `setstatus`
     *
     * - **authority**: `get_self()`
     *
     * > Withdraw balance
     *
     * ### params
     *
     * - `{bool} disabled_withdraw` - whether to disable withdrawal of balance
     *
     * ### example
     *
     * ```bash
     * $ cleos push action rescmng.xsat setstatus '[true]' -p rescmng.xsat
     * ```
     */
    [[eosio::action]]
    void setstatus(const bool disabled_withdraw);

    /**
     * ## ACTION `pay`
     *
     * - **authority**: `blksync.xsat` or `blkendt.xsat` or `utxomng.xsat` or `poolreg.xsat` or `blkendt.xsat`
     *
     * > Pay the fee.
     *
     * ### params
     *
     * - `{uint64_t} height` - block height
     * - `{hash} hash` - block hash
     * - `{name} owner` - debited account
     * - `{fee_type} type` - types of deductions
     * - `{uint64_t} quantity` - payment quantity
     *
     * ### example
     *
     * ```bash
     * $ cleos push action rescmng.xsat pay '[840000, "0000000000000000000320283a032748cef8227873ff4872689bf23f1cda83a5", "alice", 1, 1]' -p blksync.xsat
     * ```
     */
    [[eosio::action]]
    void pay(const uint64_t height, const checksum256& hash, const name& owner, const fee_type type,
             const uint64_t quantity);

    /**
     * ## ACTION `withdraw`
     *
     * - **authority**: `owner`
     *
     * > Withdraw balance
     *
     * ### params
     *
     * - `{name} owner` - account for withdrawing fees
     * - `{asset} quantity` - the number of tokens to be withdrawn
     *
     * ### example
     *
     * ```bash
     * $ cleos push action rescmng.xsat withdraw '["alice", "0.00000001 BTC"]' -p alice
     * ```
     */
    [[eosio::action]]
    void withdraw(const name& owner, const asset& quantity);

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

#ifdef DEBUG
    [[eosio::action]]
    void cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows);
#endif

    // logs
    [[eosio::action]]
    void checklog(const name& client, const uint8_t& type, const bool success, const CheckResult& result) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void depositlog(const name& owner, const asset& quantity, const asset& balance) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void withdrawlog(const name& owner, const asset& quantity, const asset& balance) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void paylog(const uint64_t height, const checksum256& hash, const name& owner, const uint8_t& type,
                const asset& quantity) {
        require_auth(get_self());
    }

    using pay_action = eosio::action_wrapper<"pay"_n, &resource_management::pay>;
    using checklog_action = eosio::action_wrapper<"checklog"_n, &resource_management::checklog>;
    using depositlog_action = eosio::action_wrapper<"depositlog"_n, &resource_management::depositlog>;
    using withdrawlog_action = eosio::action_wrapper<"withdrawlog"_n, &resource_management::withdrawlog>;
    using paylog_action = eosio::action_wrapper<"paylog"_n, &resource_management::paylog>;

   private:
    // table init
    account_table _account = account_table(_self, _self.value);
    config_table _config = config_table(_self, _self.value);
    heartbeat_table _heartbeat = heartbeat_table(_self, _self.value);
    feestat_table _feestat = feestat_table(_self, _self.value);

    // private method
    void do_deposit(const name& from, const name& contract, const asset& quantity, const string& memo);
    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);
    asset get_fee(const fee_type type, const uint64_t time_or_size);

#ifdef DEBUG
    template <typename T>
    void clear_table(T& table, uint64_t rows_to_clear);
#endif
};