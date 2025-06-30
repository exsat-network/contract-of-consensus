#pragma once

#include <eosio/eosio.hpp>
#include <eosio/name.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <eosio/binary_extension.hpp>
#include <sstream>
#include <endrmng.xsat/endrmng.xsat.hpp>
#include <btc.xsat/btc.xsat.hpp>
#include <bitcoin/utility/address_converter.hpp>
#include "../internal/defines.hpp"
#include "../internal/utils.hpp"
#include "../internal/safemath.hpp"

using namespace eosio;
using std::string;

class [[eosio::contract("custody.xsat")]] custody : public contract {

public:
    using contract::contract;

    static const uint64_t MAX_STAKING = 10000000000; // 100 BTC in satoshi

    /**
     * ## ACTION `addcustody`
     *
     * - **authority**: `get_self()`
     *
     * > Add custody record
     *
     * ### params
     *
     * - `{checksum160} staker` - staker evm address
     * - `{checksum160} proxy` - proxy evm address
     * - `{name} validator` - validator account
     * - `{optional<string>} btc_address` - bitcoin address
     * - `{optional<vector<uint8_t>>} scriptpubkey` - scriptpubkey
     *
     * ### example
     *
     * ```bash
     * $ cleos push action custody.xsat addcustody '["1231deb6f5749ef6ce6943a275a1d3e7486f4eae", "0000000000000000000000000000000000000001", "val1.xsat", "3LB8ocwXtqgq7sDfiwv3EbDZNEPwKLQcsN", null]' -p custody.xsat
     * ```
     */
    [[eosio::action]]
    void addcustody(const checksum160 staker, const checksum160 proxy, const name validator, const optional<string> btc_address, optional<vector<uint8_t>> scriptpubkey);

    /**
     * ## ACTION `delcustody`
     *
     * - **authority**: `get_self()`
     *
     * > Delete custody record
     *
     * ### params
     *
     * - `{checksum160} staker` - staker evm address
     *
     * ### example
     *
     * ```bash
     * $ cleos push action custody.xsat delcustody '["1231deb6f5749ef6ce6943a275a1d3e7486f4eae"]' -p custody.xsat
     * ```
     */
    [[eosio::action]]
    void delcustody(const checksum160 staker);

    /**
     * ## ACTION `creditstake`
     *
     * - **authority**: `get_self()`
     *
     * > Sync staker btc address stake off chain
     *
     * ### params
     *
     * - `{checksum160} staker` - staker evm address
     * - `{uint64_t} balance` - staker btc balance
     *
     * ### example
     *
     * ```bash
     * $ cleos push action custody.xsat creditstake '["1231deb6f5749ef6ce6943a275a1d3e7486f4eae", 10000000000]' -p custody.xsat
     * ```
     */
    [[eosio::action]]
    void creditstake(const checksum160& staker, const uint64_t balance);

    /**
     * ## ACTION `config`
     *
     * - **authority**: `get_self()`
     *
     * > Set the valid block range for transaction verification
     *
     * ### params
     *
     * - `{uint64_t} valid_blocks` - number of blocks considered valid for verification
     *
     * ### example
     *
     * ```bash
     * $ cleos push action custody.xsat config '[1004]' -p custody.xsat
     * ```
     */
    [[eosio::action]]
    void config(const uint64_t valid_blocks);

    /**
     * ## ACTION `enroll`
     *
     * - **authority**: `account`
     *
     * > Enroll an account in the system and return a unique identifier
     *
     * ### params
     *
     * - `{name} account` - account to enroll
     *
     * ### return
     *
     * - `{uint64_t}` - unique identifier for the enrolled account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action custody.xsat enroll '["myaccount"]' -p myaccount
     * ```
     */
    [[eosio::action]]
    uint64_t enroll(const name& account);

    /**
     * ## ACTION `verifytx`
     *
     * - **authority**: `account`
     *
     * > Verify a Bitcoin transaction for the specified account
     *
     * ### params
     *
     * - `{name} account` - account requesting verification
     * - `{string} btc_address` - Bitcoin address involved in the transaction
     * - `{checksum256} txid` - Transaction ID to verify
     * - `{string} information` - Additional transaction information
     *
     * ### example
     *
     * ```bash
     * $ cleos push action custody.xsat verifytx '["myaccount", "bc1q9h8zk36g3vajhlgvgk7x33vnygyn66fx8m0srt",
     * "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b", "transaction details"]' -p myaccount
     * ```
     */
    [[eosio::action]]
    void verifytx(const name& account, const string& btc_address, const checksum256& txid, const string& information);

    [[eosio::action]]
    void verifyresult(const name& account, const uint8_t is_valid, const string& verification_result);

#ifdef DEBUG
    [[eosio::action]]
    void cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows);
    [[eosio::action]]
    void addr2pubkey(const string& address);
    [[eosio::action]]
    void pubkey2addr(const vector<uint8_t> data);
    [[eosio::action]]
    void modifyrandom(const name& account, const uint64_t random);
#endif

private:
    /**
     * ## TABLE `globals`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} custody_id` - the latest custody id
     *
     * ### example
     *
     * ```json
     * {
     *   "custody_id": 1,
     * }
     * ```
     */
    struct [[eosio::table]] global_row {
        uint64_t custody_id;
        binary_extension<uint64_t> valid_blocks;
    };
    typedef singleton<"globals"_n, global_row> global_table;
    global_table _global = global_table(_self, _self.value);

    /**
     * ## TABLE `custodies`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} id` - the custody id
     * - `{checksum160} staker` - the staker evm address
     * - `{checksum160} proxy` - the proxy evm address
     * - `{name} validator` - the validator account
     * - `{string} btc_address` - the bitcoin address
     * - `{vector<uint8_t>} scriptpubkey` - the scriptpubkey
     * - `{uint64_t} value` - the total utxo value
     * - `{time_point_sec} latest_stake_time` - the latest stake time
     *
     * ### example
     *
     * ```
     * {
     *   "id": 7,
     *   "staker": "ee37064f01ec9314278f4984ff4b9b695eb91912",
     *   "proxy": "0000000000000000000000000000000000000001",
     *   "validator": "val1.xsat",
     *   "btc_address": "3LB8ocwXtqgq7sDfiwv3EbDZNEPwKLQcsN",
     *   "scriptpubkey": "a914cac3a79a829c31b07e6a8450c4e05c4289ab95b887"
     *   "value": 100000000,
     *   "latest_stake_time": "2021-09-01T00:00:00"
     * }
     * ```
     *
     */
    struct [[eosio::table]] custody_row {
        uint64_t id;
        checksum160 staker;
        checksum160 proxy;
        name validator;
        string btc_address;
        std::vector<uint8_t> scriptpubkey;
        uint64_t value;
        time_point_sec latest_stake_time;
        uint64_t primary_key() const { return id; }
        checksum256 by_staker() const { return xsat::utils::compute_id(staker); }
        checksum256 by_scriptpubkey() const { return xsat::utils::hash(scriptpubkey); }
    };
    typedef eosio::multi_index<"custodies"_n, custody_row,
        eosio::indexed_by<"bystaker"_n, const_mem_fun<custody_row, checksum256, &custody_row::by_staker>>,
        eosio::indexed_by<"scriptpubkey"_n, const_mem_fun<custody_row, checksum256, &custody_row::by_scriptpubkey>>>
        custody_index;

    /**
     * ## TABLE `enrollment_row`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{name} account` - the enrolled account name
     * - `{uint64_t} random` - random identifier assigned during enrollment
     * - `{string} btc_address` - the associated Bitcoin address
     * - `{checksum256} txid` - transaction ID for verification
     * - `{uint32_t} index` - index position in transaction
     * - `{uint64_t} start_height` - starting block height for validity period
     * - `{uint64_t} end_height` - ending block height for validity period
     * - `{bool} is_valid` - validation status flag
     * - `{string} information` - additional transaction information
     *
     * ### example
     *
     * ```
     * {
     *   "account": "test.sat",
     *   "random": 15309,
     *   "btc_address": "bc1qar0srrr7xfkvy5l643lydnw9re59gtzzwf5mdq",
     *   "txid": "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b",
     *   "index": 0,
     *   "start_height": 750000,
     *   "end_height": 750006,
     *   "is_valid": true,
     *   "information": "User verification transaction details"
     * }
     * ```
     *
     */
    struct [[eosio::table]] enrollment_row {
        name account;
        uint64_t random;
        string btc_address;
        checksum256 txid;
        uint32_t index;
        uint64_t start_height;
        uint64_t end_height;
        uint8_t is_valid = 0; // 0-init, 1-valid, 2-invalid
        string information;
        string verification_result;  // result of the verification
        uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"enrollments"_n, enrollment_row> enrollment_index;

    // table init
    custody_index _custody = custody_index(_self, _self.value);
    enrollment_index _enrollment = enrollment_index(_self, _self.value);

    template <typename T>
    uint64_t get_current_staking_value(T& itr);

    template <typename T>
    void handle_staking(T& itr, uint64_t balance);

    uint64_t next_custody_id();

#ifdef DEBUG
    template <typename T>
    void clear_table(T& table, uint64_t rows_to_clear);
#endif
};