#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/crypto.hpp>
#include <eosio/binary_extension.hpp>
#include "../internal/defines.hpp"
#include "../internal/utils.hpp"

using namespace eosio;
using namespace std;

class [[eosio::contract("endrmng.xsat")]] endorse_manage : public contract {
   public:
    using contract::contract;

    // CONSTANTS
    const std::set<name> WHITELIST_TYPES = {"proxyreg"_n, "evmcaller"_n};

    // 0: BTC, 1: XSAT
    const std::set<uint32_t> VALIDATOR_ROLE = {0, 1};
    
    /**
     * ## TABLE `globalid`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} staking_id` - the latest staking id
     *
     * ### example
     *
     * ```json
     * {
     *   "staking_id": 1
     * }
     * ```
     */
    struct [[eosio::table]] global_id_row {
        uint64_t staking_id;
    };
    typedef singleton<"globalid"_n, global_id_row> global_id_table;

    /**
     * ## TABLE `config`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{string} donation_account` - the account designated for receiving donations
     * - `{binary_extension<uint16_t>} min_donate_rate` - minimum donation rate
     * 
     * ### example
     *
     * ```json
     * {
     *   "donation_account": "donate.xsat",
     *   "min_donate_rate": 2000
     * }
     * ```
     */
    struct [[eosio::table]] config_row {
        string donation_account;
        binary_extension<uint16_t> min_donate_rate;
    };
    typedef eosio::singleton<"config"_n, config_row> config_table;

    /**
     * ## TABLE `whitelist`
     *
     * ### scope `proxyreg` or `evmcaller`
     * ### params
     *
     * - `{name} account` - whitelist account
     *
     * ### example
     *
     * ```json
     * {
     *   "account": "alice"
     * }
     * ```
     */
    struct [[eosio::table]] whitelist_row {
        name account;
        uint64_t primary_key() const { return account.value; }
    };
    typedef eosio::multi_index<"whitelist"_n, whitelist_row> whitelist_table;

    /**
     * ## TABLE `evmproxies`
     *
     * ### scope the account whose scope is evmcaller in the `whitelist` table
     * ### params
     *
     * - `{uint64_t} id` - evm proxy id
     * - `{checksum160} proxy` - evm proxy account
     *
     * ### example
     *
     * ```json
     * {
     *   "id": 1,
     *   "proxy": "bb776ae86d5996908af46482f24be8ccde2d4c41"
     * }
     * ```
     */
    struct [[eosio::table]] evm_proxy_row {
        uint64_t id;
        checksum160 proxy;
        uint64_t primary_key() const { return id; }
        checksum256 by_proxy() const { return xsat::utils::compute_id(proxy); }
    };
    typedef eosio::multi_index<
        "evmproxies"_n, evm_proxy_row,
        eosio::indexed_by<"byproxy"_n, const_mem_fun<evm_proxy_row, checksum256, &evm_proxy_row::by_proxy>>>
        evm_proxy_table;

    /**
     * ## TABLE `creditproxy`
     *
     * ### scope the account whose scope is evmcaller in the `whitelist` table
     * ### params
     *
     * - `{uint64_t} id` - evm proxy id
     * - `{checksum160} proxy` - evm proxy account
     *
     * ### example
     *
     * ```json
     * {
     *   "id": 1,
     *   "proxy": "bb776ae86d5996908af46482f24be8ccde2d4c41"
     * }
     * ```
     */
    struct [[eosio::table]] credit_proxy_row {
        uint64_t id;
        checksum160 proxy;
        uint64_t primary_key() const { return id; }
        checksum256 by_proxy() const { return xsat::utils::compute_id(proxy); }
    };
    typedef eosio::multi_index<
        "creditproxy"_n, credit_proxy_row,
        eosio::indexed_by<"byproxy"_n, const_mem_fun<credit_proxy_row, checksum256, &credit_proxy_row::by_proxy>>>
        credit_proxy_table;

    /**
     * ## TABLE `evmstakers`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} id` - evm staker id
     * - `{checksum160} proxy` - proxy account
     * - `{checksum160} staker` - staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - total number of staking
     * - `{asset} xsat_quantity` - the amount of XSAT tokens staked
     * - `{asset} total_donated` - the total amount of XSAT that has been donated
     * - `{uint64_t} stake_debt` - amount of requested stake debt
     * - `{asset} staking_reward_unclaimed` - amount of stake unclaimed rewards
     * - `{asset} staking_reward_claimed` - amount of stake claimed rewards
     * - `{uint64_t} consensus_debt` - amount of requested consensus debt
     * - `{asset} consensus_reward_unclaimed` - amount of consensus unclaimed rewards
     * - `{asset} consensus_reward_claimed` - amount of consensus claimed rewards
     *
     * ### example
     *
     * ```json
     * {
     *   "id": 4,
     *   "proxy": "bb776ae86d5996908af46482f24be8ccde2d4c41",
     *   "staker": "e4d68a77714d9d388d8233bee18d578559950cf5",
     *   "validator": "alice",
     *   "quantity": "0.10000000 BTC",
     *   "xsat_quantity": "0.10000000 XSAT",
     *   "total_donated": "1.00000000 XSAT",
     *   "stake_debt": 1385452,
     *   "staking_reward_unclaimed": "0.00000000 XSAT",
     *   "staking_reward_claimed": "0.00000000 XSAT",
     *   "consensus_debt": 173181,
     *   "consensus_reward_unclaimed": "0.00000000 XSAT",
     *   "consensus_reward_claimed": "0.00000000 XSAT"
     * }
     * ```
     */
    struct [[eosio::table]] evm_staker_row {
        uint64_t id;
        checksum160 proxy;
        checksum160 staker;
        name validator;
        asset quantity;
        asset xsat_quantity;
        asset total_donated;
        uint64_t stake_debt;
        asset staking_reward_unclaimed;
        asset staking_reward_claimed;
        uint64_t consensus_debt;
        asset consensus_reward_unclaimed;
        asset consensus_reward_claimed;
        uint64_t primary_key() const { return id; }
        uint64_t by_validator() const { return validator.value; }
        checksum256 by_staker() const { return xsat::utils::compute_id(staker); }
        checksum256 by_proxy() const { return xsat::utils::compute_id(proxy); }
        checksum256 by_staking_id() const { return compute_staking_id(proxy, staker, validator); }
    };
    typedef eosio::multi_index<
        "evmstakers"_n, evm_staker_row,
        eosio::indexed_by<"byvalidator"_n, const_mem_fun<evm_staker_row, uint64_t, &evm_staker_row::by_validator>>,
        eosio::indexed_by<"bystaker"_n, const_mem_fun<evm_staker_row, checksum256, &evm_staker_row::by_staker>>,
        eosio::indexed_by<"bystakingid"_n, const_mem_fun<evm_staker_row, checksum256, &evm_staker_row::by_staking_id>>,
        eosio::indexed_by<"byproxy"_n, const_mem_fun<evm_staker_row, checksum256, &evm_staker_row::by_proxy>>>
        evm_staker_table;

    /**
     * ## TABLE `stakers`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} id` - staker id
     * - `{name} staker` - staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - total number of staking
     * - `{asset} xsat_quantity` - the amount of XSAT tokens staked
     * - `{asset} total_donated` - the total amount of XSAT that has been donated
     * - `{uint64_t} stake_debt` - amount of requested stake debt
     * - `{asset} staking_reward_unclaimed` - amount of stake unclaimed rewards
     * - `{asset} staking_reward_claimed` - amount of stake claimed rewards
     * - `{uint64_t} consensus_debt` - amount of requested consensus debt
     * - `{asset} consensus_reward_unclaimed` - amount of consensus unclaimed rewards
     * - `{asset} consensus_reward_claimed` - amount of consensus claimed rewards
     *
     * ### example
     *
     * ```json
     * {
     *   "id": 2,
     *   "staker": "alice",
     *   "validator": "alice",
     *   "quantity": "0.10000000 BTC",
     *   "xsat_quantity": "0.10000000 XSAT",
     *   "total_donated": "1.00000000 XSAT",
     *   "stake_debt": 1385452,
     *   "staking_reward_unclaimed": "0.00000000 XSAT",
     *   "staking_reward_claimed": "0.00000000 XSAT",
     *   "consensus_debt": 173181,
     *   "consensus_reward_unclaimed": "0.00000000 XSAT",
     *   "consensus_reward_claimed": "0.00000000 XSAT"
     * }
     * ```
     */
    struct [[eosio::table]] native_staker_row {
        uint64_t id;
        name staker;
        name validator;
        asset quantity;
        asset xsat_quantity;
        asset total_donated;
        uint64_t stake_debt;
        asset staking_reward_unclaimed;
        asset staking_reward_claimed;
        uint64_t consensus_debt;
        asset consensus_reward_unclaimed;
        asset consensus_reward_claimed;
        uint64_t primary_key() const { return id; }
        uint64_t by_validator() const { return validator.value; }
        uint64_t by_staker() const { return staker.value; }
        uint128_t by_staking_id() const { return compute_staking_id(staker, validator); }
    };
    typedef eosio::multi_index<
        "stakers"_n, native_staker_row,
        eosio::indexed_by<"byvalidator"_n,
                          const_mem_fun<native_staker_row, uint64_t, &native_staker_row::by_validator>>,
        eosio::indexed_by<"bystaker"_n, const_mem_fun<native_staker_row, uint64_t, &native_staker_row::by_staker>>,
        eosio::indexed_by<"bystakingid"_n,
                          const_mem_fun<native_staker_row, uint128_t, &native_staker_row::by_staking_id>>>
        native_staker_table;

    /**
     * ## TABLE `validators`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{name} owner` - staker id
     * - `{name} reward_recipient` - receiving account for receiving rewards
     * - `{string} memo` - memo when receiving reward transfer
     * - `{uint16_t} commission_rate` - commission ratio, decimal is 10^4
     * - `{asset} quantity` - the amount of BTC staked by the validator
     * - `{asset} qualification` -  the qualification of the validator
     * - `{asset} xsat_quantity` - the amount of XSAT tokens staked by the validator
     * - `{uint16_t} donate_rate` - the donation rate, represented as a percentage, ex: 500 means 5.00%
     * - `{asset} total_donated` - the total amount of XSAT that has been donated
     * - `{uint128_t} stake_acc_per_share` - staking rewards earnings per share
     * - `{uint128_t} consensus_acc_per_share` - consensus reward earnings per share
     * - `{asset} staking_reward_unclaimed` - unclaimed staking rewards
     * - `{asset} staking_reward_claimed` - amount of stake claimed rewards
     * - `{asset} consensus_reward_unclaimed` - amount of consensus unclaimed rewards
     * - `{asset} consensus_reward_claimed` - amount of consensus claimed rewards
     * - `{asset} total_consensus_reward` - total consensus rewards
     * - `{asset} consensus_reward_balance` - consensus reward balance
     * - `{asset} total_staking_reward` - total staking rewards
     * - `{asset} staking_reward_balance` - staking reward balance
     * - `{time_point_sec} latest_staking_time` - latest staking or unstaking time
     * - `{uint64_t} latest_reward_block` - latest reward block
     * - `{time_point_sec} latest_reward_time` - latest reward time
     * - `{bool} disabled_staking` - whether to disable staking
     *
     * ### example
     *
     * ```json
     * {
     *   "owner": "alice",
     *   "reward_recipient": "erc2o.xsat",
     *   "memo": "0x5EB954fB68159e0b7950936C6e1947615b75C895",
     *   "commission_rate": 0,
     *   "quantity": "102.10000000 BTC",
     *   "qualification": "102.10000000 BTC",
     *   "xsat_quantity": "1000.10000000 XSAT",
     *   "donate_rate": 100,
     *   "total_donated": "100.00000000 XSAT",
     *   "stake_acc_per_share": "39564978",
     *   "consensus_acc_per_share": "4945621",
     *   "staking_reward_unclaimed": "0.00000000 XSAT",
     *   "staking_reward_claimed": "0.00000000 XSAT",
     *   "consensus_reward_unclaimed": "0.00000000 XSAT",
     *   "consensus_reward_claimed": "0.00000000 XSAT",
     *   "total_consensus_reward": "5.04700642 XSAT",
     *   "consensus_reward_balance": "5.04700642 XSAT",
     *   "total_staking_reward": "40.37605144 XSAT",
     *   "staking_reward_balance": "40.37605144 XSAT",
     *   "latest_staking_time": "2024-07-13T09:16:26",
     *   "latest_reward_block": 840001,
     *   "latest_reward_time": "2024-07-13T14:29:32",
     *   "disabled_staking": 0
     *  }
     * ```
     */
    struct [[eosio::table]] validator_row {
        name owner;
        name reward_recipient;
        string memo;
        uint16_t commission_rate;
        asset quantity;
        asset qualification;
        asset xsat_quantity;
        uint16_t donate_rate;
        asset total_donated;
        uint128_t stake_acc_per_share;
        uint128_t consensus_acc_per_share;
        asset staking_reward_unclaimed;
        asset staking_reward_claimed;
        asset consensus_reward_unclaimed;
        asset consensus_reward_claimed;
        asset total_consensus_reward;
        asset consensus_reward_balance;
        asset total_staking_reward;
        asset staking_reward_balance;
        time_point_sec latest_staking_time;
        uint64_t latest_reward_block;
        time_point_sec latest_reward_time;
        bool disabled_staking;
        uint64_t primary_key() const { return owner.value; }
        uint64_t by_btc_total_staking() const { return quantity.amount; }
        uint64_t by_xsat_total_staking() const { return xsat_quantity.amount; }
        uint64_t by_qualification() const { return qualification.amount; }
        uint64_t by_disabled_staking() const { return disabled_staking; }
        uint64_t by_total_donated() const { return total_donated.amount; }

        // v2
        binary_extension<checksum160> stake_address;
        binary_extension<checksum160> reward_address;
        binary_extension<uint64_t> consecutive_vote_count;
        binary_extension<uint64_t> latest_consensus_block;

        // 0: inactive, 1: active, 2: credit staking validator
        binary_extension<uint8_t> active_flag;

        // 0: BTC, 1: XSAT
        binary_extension<uint32_t> role;

        uint64_t get_role() const {
            return (role.has_value() && role.value() == 1) ? (XSAT_SCOPE_MASK | (owner.value >> 32)) : (owner.value >> 32);
        }

        checksum256 by_stake_address() const {
            // if (!stake_address.has_value()) {
            //     return checksum256();
            // }
            // checksum160 addr = stake_address.value();
            // return xsat::utils::compute_id(addr);
            return xsat::utils::compute_id(stake_address.value_or());
        }

    };
    typedef eosio::multi_index<
        "validators"_n, validator_row,
        eosio::indexed_by<"bystakedbtc"_n,
                          const_mem_fun<validator_row, uint64_t, &validator_row::by_btc_total_staking>>,
        eosio::indexed_by<"bystakedxsat"_n,
                          const_mem_fun<validator_row, uint64_t, &validator_row::by_xsat_total_staking>>,
        eosio::indexed_by<"byqualifictn"_n, const_mem_fun<validator_row, uint64_t, &validator_row::by_qualification>>,
        eosio::indexed_by<"bydonate"_n, const_mem_fun<validator_row, uint64_t, &validator_row::by_total_donated>>,
        eosio::indexed_by<"byrole"_n, const_mem_fun<validator_row, uint64_t, &validator_row::get_role>>,
        eosio::indexed_by<"bystakeaddr"_n, const_mem_fun<validator_row, checksum256, &validator_row::by_stake_address>>>
        validator_table;

    /**
     * ## TABLE `stat`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{asset} total_staking` - btc total staking amount
     * - `{asset} xsat_total_staking` - the total amount of XSAT staked 
     * - `{asset} xsat_total_donated` - the cumulative amount of XSAT donated
     *
     * ### example
     *
     * ```json
     * {
     *   "total_staking": "100.40000000 BTC",
     *   "xsat_total_staking": "100.40000000 XSAT",
     *   "xsat_total_donated": "100.40000000 XSAT"
     * }
     * ```
     */
    struct [[eosio::table]] stat_row {
        asset total_staking = {0, BTC_SYMBOL};
        asset xsat_total_staking = {0, XSAT_SYMBOL};
        asset xsat_total_donated = {0, XSAT_SYMBOL};
    };
    typedef eosio::singleton<"stat"_n, stat_row> stat_table;

    /**
     * ## ACTION `setdonateacc`
     *
     * - **authority**: `get_self()`
     *
     * > Update donation account.
     *
     * ### params
     *
     * - `{string} donation_account` -  account to receive donations
     * - `{uint16_t} min_donate_rate` - minimum donation rate 
     * 
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat setdonateacc '["alice", 2000]' -p endrmng.xsat
     * ```
     */
    void setdonateacc(const string& donation_account, const uint16_t min_donate_rate);

    /**
     * ## ACTION `addwhitelist`
     *
     * - **authority**: `get_self()`
     *
     * > Add whitelist account
     *
     * ### params
     *
     * - `{name} type` - whitelist type @see `WHITELIST_TYPES`
     * - `{name} account` - whitelist account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat addwhitelist '["proxyreg", "alice"]' -p endrmng.xsat
     * ```
     */
    [[eosio::action]]
    void addwhitelist(const name& type, const name& account);

    /**
     * ## ACTION `delwhitelist`
     *
     * - **authority**: `get_self()`
     *
     * > Delete whitelist account
     *
     * ### params
     *
     * - `{name} type` - whitelist type @see `WHITELIST_TYPES`
     * - `{name} account` - whitelist account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat addwhitelist '["proxyreg", "alice"]' -p endrmng.xsat
     * ```
     */
    [[eosio::action]]
    void delwhitelist(const name& type, const name& account);

    /**
     * ## ACTION `addevmproxy`
     *
     * - **authority**: `get_self()`
     *
     * > Add evm proxy account
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - proxy account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat addevmproxy '["evmcaller", "bb776ae86d5996908af46482f24be8ccde2d4c41"]' -p endrmng.xsat
     * ```
     */
    [[eosio::action]]
    void addevmproxy(const name& caller, const checksum160& proxy);

    /**
     * ## ACTION `delevmproxy`
     *
     * - **authority**: `get_self()`
     *
     * > Delete evm proxy account
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - proxy account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat delevmproxy '["evmcaller", "bb776ae86d5996908af46482f24be8ccde2d4c41"]' -p endrmng.xsat
     * ```
     */
    [[eosio::action]]
    void delevmproxy(const name& caller, const checksum160& proxy);

    /**
     * ## ACTION `addcrdtproxy`
     *
     * - **authority**: `get_self()`
     *
     * > Add credit proxy account
     *
     * ### params
     *
     * - `{checksum160} proxy` - proxy account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat addcrdtproxy '["bb776ae86d5996908af46482f24be8ccde2d4c41"]' -p endrmng.xsat
     * ```
     */
    [[eosio::action]]
    void addcrdtproxy(const checksum160& proxy);

    /**
     * ## ACTION `delcrdtproxy`
     *
     * - **authority**: `get_self()`
     *
     * > Delete credit proxy account
     *
     * ### params
     *
     * - `{checksum160} proxy` - proxy account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat delcrdtproxy '["bb776ae86d5996908af46482f24be8ccde2d4c41"]' -p endrmng.xsat
     * ```
     */
    [[eosio::action]]
    void delcrdtproxy(const checksum160& proxy);

    /**
     * ## ACTION `setstatus`
     *
     * - **authority**: `get_self()`
     *
     * > Set validator staking status
     *
     * ### params
     *
     * - `{name} validator` - validator account
     * - `{bool} disabled_staking` - whether to disable staking
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat setstatus '["alice",  true]' -p alice
     * ```
     */
    [[eosio::action]]
    void setstatus(const name& validator, const bool disabled_staking);

    /**
     * ## ACTION `regvalidator`
     *
     * - **authority**: `validator`
     *
     * > Registering a validator
     *
     * ### params
     *
     * - `{name} validator` - validator account
     * - `{name} financial_account` - financial accounts
     * - `{uint16_t} commission_rate` - commission ratio, decimal is 10^4
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat regvalidator '["alice", "alice", 1000]' -p alice
     * ```
     */
    [[eosio::action]]
    void regvalidator(const name& validator, const string& financial_account, const uint16_t commission_rate);

    /**
     * ## ACTION `proxyreg`
     *
     * - **authority**: `proxy`
     *
     * > Proxy account registration validator
     *
     * ### params
     *
     * - `{name} proxy` - proxy account
     * - `{name} validator` - validator account
     * - `{string} financial_account` - financial accounts
     * - `{uint16_t} commission_rate` - commission ratio, decimal is 10^4
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat proxyreg '["test.xsat", "alice", "alice",  1000]' -p test.xsat
     * ```
     */
    [[eosio::action]]
    void proxyreg(const name& proxy, const name& validator, const string& financial_account,
                  const uint16_t commission_rate);

    /**
     * ## ACTION `config`
     *
     * - **authority**: `validator`
     *
     * > Validator sets commission ratio and financial account
     *
     * ### params
     *
     * - `{name} validator` - validator account
     * - `{optional<uint16_t>} commission_rate` - commission ratio, decimal is 10^4
     * - `{optional<string>} financial_account` - financial accounts
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat config '["alice",  1000, "alice"]' -p alice
     * ```
     */
    [[eosio::action]]
    void config(const name& validator, const optional<uint16_t>& commission_rate,
                const optional<string>& financial_account);

    /**
     * ## ACTION `setdonate`
     *
     * - **authority**: `validator`
     *
     * > Configure donate rate.
     *
     * ### params
     *
     * - `{name} validator` - synchronizer account
     * - `{uint16_t} donate_rate` - the donation rate, represented as a percentage, ex: 500 means 5.00% 
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat setdonate '["alice", 100]' -p alice
     * ```
     */
    [[eosio::action]]
    void setdonate(const name& validator, const uint16_t donate_rate);

    /**
     * ## ACTION `stake`
     *
     * - **authority**: `staking.xsat`
     *
     * > Staking BTC to validator
     *
     * ### params
     *
     * - `{name} staker` - staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat stake '["alice",  "alice", "1.00000000 BTC"]' -p staking.xsat
     * ```
     */
    [[eosio::action]]
    void stake(const name& staker, const name& validator, const asset& quantity);

    /**
     * ## ACTION `unstake`
     *
     * - **authority**: `staking.xsat`
     *
     * > Unstaking BTC from a validator
     *
     * ### params
     *
     * - `{name} staker` - staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - cancel staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat unstake '["alice",  "alice", "1.00000000 BTC"]' -p staking.xsat
     * ```
     */
    [[eosio::action]]
    void unstake(const name& staker, const name& validator, const asset& quantity);

    /**
     * ## ACTION `newstake`
     *
     * - **authority**: `staker`
     *
     * > Staking BTC to a new validator
     *
     * ### params
     *
     * - `{name} staker` - staker account
     * - `{name} old_validator` - old validator account
     * - `{name} new_validator` - new validator account
     * - `{asset} quantity` - the amount of stake transferred to the new validator
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat newstake '["alice",  "alice", "bob", "1.00000000 BTC"]' -p alice
     * ```
     */
    [[eosio::action]]
    void newstake(const name& staker, const name& old_validator, const name& new_validator, const asset& quantity);

    /**
     * ## ACTION `claim`
     *
     * - **authority**: `staker`
     *
     * > Claim staking rewards
     *
     * ### params
     *
     * - `{name} staker` - staker account
     * - `{name} validator` - validator account
     * - `{uint16_t} donate_rate` - the donation rate, represented as a percentage, ex: 500 means 5.00%
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat claim '["alice",  "bob", 100]' -p alice
     * ```
     */
    [[eosio::action]]
    void claim(const name& staker, const name& validator, const uint16_t donate_rate);

    /**
     * ## ACTION `evmstake`
     *
     * - **authority**: `caller`
     *
     * > Staking BTC to validator via evm
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat evmstake '["evmutil.xsat", "bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice", "1.00000000 BTC"]' -p evmutil.xsat
     * ```
     */
    [[eosio::action]]
    void evmstake(const name& caller, const checksum160& proxy, const checksum160& staker, const name& validator,
                  const asset& quantity);

    /**
     * ## ACTION `evmunstake`
     *
     * - **authority**: `caller`
     *
     * > Unstake BTC from validator via evm
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat evmunstake '["evmutil.xsat", "bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice", "1.00000000 BTC"]' -p evmutil.xsat
     * ```
     */
    [[eosio::action]]
    void evmunstake(const name& caller, const checksum160& proxy, const checksum160& staker, const name& validator,
                    const asset& quantity);

    /**
     * ## ACTION `evmnewstake`
     *
     * - **authority**: `caller`
     *
     * > Staking BTC to a new validator via evm
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} old_validator` - old validator account
     * - `{name} new_validator` - new validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat evmnewstake '["evmutil.xsat", "bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice", "bob", "1.00000000 BTC"]' -p evmutil.xsat
     * ```
     */
    [[eosio::action]]
    void evmnewstake(const name& caller, const checksum160& proxy, const checksum160& staker, const name& old_validator,
                     const name& new_validator, const asset& quantity);

    /**
     * ## ACTION `evmclaim`
     *
     * - **authority**: `caller`
     *
     * > Claim staking rewards through evm
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} validator` - validator account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat evmclaim '["evmutil.xsat", "bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice"]' -p evmutil.xsat
     * ```
     */
    [[eosio::action]]
    void evmclaim(const name& caller, const checksum160& proxy, const checksum160& staker, const name& validator);

    /**
     * ## ACTION `evmclaim2`
     *
     * - **authority**: `caller`
     *
     * > Claim staking rewards through evm
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} validator` - validator account
     * - `{uint16_t} donate_rate` - the donation rate, represented as a percentage, ex: 500 means 5.00%
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat evmclaim2 '["evmutil.xsat", "bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice", 100]' -p evmutil.xsat
     * ```
     */
    [[eosio::action]]
    void evmclaim2(const name& caller, const checksum160& proxy, const checksum160& staker, const name& validator,
                   const uint16_t donate_rate);

    /**
     * ## ACTION `vdrclaim`
     *
     * - **authority**: `validator->reward_recipient` or `evmutil.xsat`
     *
     * > Validator Receive Rewards
     *
     * ### params
     *
     * - `{name} validator` - validator account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat vdrclaim '["alice"]' -p alice
     * ```
     */
    [[eosio::action]]
    void vdrclaim(const name& validator);

    /**
     * ## ACTION `stakexsat`
     *
     * - **authority**: `xsatstk.xsat`
     *
     * > Staking XSAT to validator
     *
     * ### params
     *
     * - `{name} staker` - staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat stakexsat '["alice",  "alice", "1.00000000 XSAT"]' -p xsatstk.xsat
     * ```
     */
    [[eosio::action]]
    void stakexsat(const name& staker, const name& validator, const asset& quantity);

    /**
     * ## ACTION `unstakexsat`
     *
     * - **authority**: `xsatstk.xsat`
     *
     * > Unstaking XSAT from a validator
     *
     * ### params
     *
     * - `{name} staker` - staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - cancel staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat unstakexsat '["alice",  "alice", "1.00000000 XSAT"]' -p xsatstk.xsat
     * ```
     */
    [[eosio::action]]
    void unstakexsat(const name& staker, const name& validator, const asset& quantity);

    /**
     * ## ACTION `restakexsat`
     *
     * - **authority**: `staker`
     *
     * > Staking XSAT to a new validator
     *
     * ### params
     *
     * - `{name} staker` - staker account
     * - `{name} old_validator` - old validator account
     * - `{name} new_validator` - new validator account
     * - `{asset} quantity` - the amount of stake transferred to the new validator
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat restakexsat '["alice",  "alice", "bob", "1.00000000 XSAT"]' -p alice
     * ```
     */
    [[eosio::action]]
    void restakexsat(const name& staker, const name& old_validator, const name& new_validator, const asset& quantity);

    /**
     * ## ACTION `evmstakexsat`
     *
     * - **authority**: `caller`
     *
     * > Staking XSAT to validator via evm
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat evmstakexsat '["evmutil.xsat", "bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice", "1.00000000 XSAT"]' -p evmutil.xsat
     * ```
     */
    [[eosio::action]]
    void evmstakexsat(const name& caller, const checksum160& proxy, const checksum160& staker, const name& validator,
                      const asset& quantity);

    /**
     * ## ACTION `evmunstkxsat`
     *
     * - **authority**: `caller`
     *
     * > Unstake XSAT from validator via evm
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat evmunstkxsat '["evmutil.xsat", "bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice", "1.00000000 XSAT"]' -p evmutil.xsat
     * ```
     */
    [[eosio::action]]
    void evmunstkxsat(const name& caller, const checksum160& proxy, const checksum160& staker, const name& validator,
                      const asset& quantity);

    /**
     * ## ACTION `evmrestkxsat`
     *
     * - **authority**: `caller`
     *
     * > Staking XSAT to a new validator via evm
     *
     * ### params
     *
     * - `{name} caller` - caller account
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} old_validator` - old validator account
     * - `{name} new_validator` - new validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat evmrestkxsat '["evmutil.xsat", "bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice", "bob", "1.00000000 XSAT"]' -p evmutil.xsat
     * ```
     */
    [[eosio::action]]
    void evmrestkxsat(const name& caller, const checksum160& proxy, const checksum160& staker,
                      const name& old_validator, const name& new_validator, const asset& quantity);

    /**
     * ## ACTION `creditstake`
     *
     * - **authority**: `custody.xsat`
     *
     * > Unstake BTC from validator via credit
     *
     * ### params
     *
     * - `{checksum160} proxy` - evm proxy account
     * - `{checksum160} staker` - evm staker account
     * - `{name} validator` - validator account
     * - `{asset} quantity` - staking amount
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat creditstake '["bb776ae86d5996908af46482f24be8ccde2d4c41", "e4d68a77714d9d388d8233bee18d578559950cf5",  "alice", "1.00000000 BTC"]' -p custody.xsat
     * ```
     */
    [[eosio::action]] [[eosio::action]]
    void creditstake(const checksum160& proxy, const checksum160& staker, const name& validator, const asset& quantity);

    /**
     * ## STRUCT `reward_details_row`
     *
     * ### params
     *
     * - `{name} validator` - validator account
     * - `{asset} staking_rewards` - staking rewards
     * - `{asset} consensus_rewards` - consensus rewards
     *
     * ### example
     *
     * ```json
     * {
     *   "validator": "alice",
     *   "staking_rewards": "0.00000010 XSAT",
     *   "consensus_rewards": "0.00000020 XSAT"
     * }
     * ```
     */
    struct reward_details_row {
        name validator;
        asset staking_rewards;
        asset consensus_rewards;
    };

    /**
     * ## ACTION `distribute`
     *
     * - **authority**: `rwddist.xsat`
     *
     * > Distributing validator rewards
     *
     * ### params
     *
     * - `{uint64_t} height` - validator account
     * - `{vector<reward_details_row>} rewards` - validator account
     *
     * ### example
     *
     * ```bash
     * $ cleos push action endrmng.xsat distribute '[840000, [{"validator": "alice", "staking_rewards": "0.00000020 XSAT", "consensus_rewards": "0.00000020 XSAT"}]]' -p rwddist.xsat
     * ```
     */
    [[eosio::action]]
    void distribute(const uint64_t height, const vector<reward_details_row> rewards);

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

#ifdef DEBUG
    [[eosio::action]]
    void cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows);

    [[eosio::action]]
    void delvalidator(const name& validator) {
        require_auth(get_self());
        auto itr = _validator.find(validator.value);
        check(itr != _validator.end(), "validator not found");
        _validator.erase(itr);
    }

    [[eosio::action]]
    void imtvalidator(const vector<validator_row>& validators) {
        require_auth(get_self());

        // batch import data to old validators
        for (const auto& validator : validators) {
            _validator.emplace(get_self(), [&](auto& row) { row = validator; });
        }
    }
#endif

    // v2
    [[eosio::action]]
    void newregvldtor(const name& validator, const uint32_t role, const checksum160& stake_addr,
                      const optional<checksum160>& reward_addr, const optional<uint16_t>& commission_rate);

    [[eosio::action]]
    void evmconfigvald(const name& validator, const optional<uint16_t>& commission_rate, const optional<uint16_t>& donate_rate);

    [[eosio::action]]
    void evmsetstaker(const name& validator, const checksum160& stake_addr);

    [[eosio::action]]
    void setrwdaddr(const name& validator, const checksum160& reward_addr);

    [[eosio::action]]
    void setstakebase(const asset& xsat_base_stake, const asset& btc_base_stake);

    [[eosio::action]]
    void updcreditstk(const bool is_close);

    [[eosio::action]]
    void endorse(const name& validator, const uint64_t height);

    // logs
    [[eosio::action]]
    void validatorlog(const name& proxy, const name& validator, const string& financial_account,
                      const uint16_t commission_rate) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void stakelog(const name& staker, const name& validator, const asset& quantity, const asset& validator_staking,
                  const asset& validator_qualification) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void unstakelog(const name& staker, const name& validator, const asset& quantity, const asset& validator_staking,
                    const asset& validator_qualification) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void newstakelog(const name& staker, const name& old_validator, const name& new_validator, const asset& quantity,
                     const asset& old_validator_staking, const asset& old_validator_qualification,
                     const asset& new_validator_staking, const asset& new_validator_qualification) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void claimlog(const name& staker, const name& validator, const asset& quantity, const asset& donated_amount,
                  const asset& total_donated) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void configlog(const name& validator, const uint16_t commission_rate, const string& financial_account) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void setdonatelog(const name& validator, const uint16_t donate_rate) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void evmstakelog(const checksum160& proxy, const checksum160& staker, const name& validator, const asset& quantity,
                     const asset& validator_staking, const asset& validator_qualification) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void evmunstlog(const checksum160& proxy, const checksum160& staker, const name& validator, const asset& quantity,
                    const asset& validator_staking, const asset& validator_qualification) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void evmnewstlog(const checksum160& proxy, const checksum160& staker, const name& old_validator,
                     const name& new_validator, const asset& quantity, const asset& old_validator_staking,
                     const asset& old_validator_qualification, const asset& new_validator_staking,
                     const asset& new_validator_qualification) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void evmclaimlog(const checksum160& proxy, const checksum160& staker, const name& validator, const asset& quantity,
                     const asset& staker_donated_amount, const asset& validator_donated_amount,
                     const asset& staker_total_donated, const asset& validator_total_donated) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void vdrclaimlog(const name& validator, const string& reward_recipient, const asset& quantity,
                     const asset& donated_amount, const asset& total_donated) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void stakexsatlog(const name& staker, const name& validator, const asset& quantity,
                      const asset& validator_staking) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void unstkxsatlog(const name& staker, const name& validator, const asset& quantity,
                      const asset& validator_staking) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void restkxsatlog(const name& staker, const name& old_validator, const name& new_validator, const asset& quantity,
                      const asset& old_validator_staking, const asset& new_validator_staking) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void estkxsatlog(const checksum160& proxy, const checksum160& staker, const name& validator, const asset& quantity,
                     const asset& validator_staking) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void eustkxsatlog(const checksum160& proxy, const checksum160& staker, const name& validator, const asset& quantity,
                      const asset& validator_staking) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void erstkxsatlog(const checksum160& proxy, const checksum160& staker, const name& old_validator,
                      const name& new_validator, const asset& quantity, const asset& old_validator_staking,
                      const asset& new_validator_staking) {
        require_auth(get_self());
    }
    
    [[eosio::action]]
    void regvldtorlog(const name& validator, const uint32_t role, const checksum160& stake_addr,
                      const optional<checksum160>& reward_addr, const optional<uint16_t>& commission_rate) {
        require_auth(get_self());
    }

    using stake_action = eosio::action_wrapper<"stake"_n, &endorse_manage::stake>;
    using unstake_action = eosio::action_wrapper<"unstake"_n, &endorse_manage::unstake>;
    using evm_stake_action = eosio::action_wrapper<"evmstake"_n, &endorse_manage::evmstake>;
    using evm_unstake_action = eosio::action_wrapper<"evmunstake"_n, &endorse_manage::evmunstake>;
    using evm_newstake_action = eosio::action_wrapper<"evmnewstake"_n, &endorse_manage::evmnewstake>;
    using creditstake_action = eosio::action_wrapper<"creditstake"_n, &endorse_manage::creditstake>;

    using stakexsat_action = eosio::action_wrapper<"stakexsat"_n, &endorse_manage::stakexsat>;
    using unstakexsat_action = eosio::action_wrapper<"unstakexsat"_n, &endorse_manage::unstakexsat>;
    using restakexsat_action = eosio::action_wrapper<"restakexsat"_n, &endorse_manage::restakexsat>;
    using evm_xsat_stake_action = eosio::action_wrapper<"evmstakexsat"_n, &endorse_manage::evmstakexsat>;
    using evm_xsat_unstake_action = eosio::action_wrapper<"evmunstkxsat"_n, &endorse_manage::evmunstkxsat>;
    using evm_xsat_restake_action = eosio::action_wrapper<"evmrestkxsat"_n, &endorse_manage::evmrestkxsat>;

    using configlog_action = eosio::action_wrapper<"configlog"_n, &endorse_manage::configlog>;
    using setdonatelog_action = eosio::action_wrapper<"setdonatelog"_n, &endorse_manage::setdonatelog>;
    using distribute_action = eosio::action_wrapper<"distribute"_n, &endorse_manage::distribute>;
    using validatorlog_action = eosio::action_wrapper<"validatorlog"_n, &endorse_manage::validatorlog>;
    using stakelog_action = eosio::action_wrapper<"stakelog"_n, &endorse_manage::stakelog>;
    using unstakelog_action = eosio::action_wrapper<"unstakelog"_n, &endorse_manage::unstakelog>;
    using newstakelog_action = eosio::action_wrapper<"newstakelog"_n, &endorse_manage::newstakelog>;
    using claimlog_action = eosio::action_wrapper<"claimlog"_n, &endorse_manage::claimlog>;
    using evmstakelog_action = eosio::action_wrapper<"evmstakelog"_n, &endorse_manage::evmstakelog>;
    using evmunstlog_action = eosio::action_wrapper<"evmunstlog"_n, &endorse_manage::evmunstlog>;
    using evmnewstlog_action = eosio::action_wrapper<"evmnewstlog"_n, &endorse_manage::evmnewstlog>;
    using evmclaimlog_action = eosio::action_wrapper<"evmclaimlog"_n, &endorse_manage::evmclaimlog>;
    using vdrclaimlog_action = eosio::action_wrapper<"vdrclaimlog"_n, &endorse_manage::vdrclaimlog>;

    using stakexsatlog_action = eosio::action_wrapper<"stakexsatlog"_n, &endorse_manage::stakexsatlog>;
    using unstkxsatlog_action = eosio::action_wrapper<"unstkxsatlog"_n, &endorse_manage::unstkxsatlog>;
    using restkxsatlog_action = eosio::action_wrapper<"restkxsatlog"_n, &endorse_manage::restkxsatlog>;
    using estkxsatlog_action = eosio::action_wrapper<"estkxsatlog"_n, &endorse_manage::estkxsatlog>;
    using eustkxsatlog_action = eosio::action_wrapper<"eustkxsatlog"_n, &endorse_manage::eustkxsatlog>;
    using erstkxsatlog_action = eosio::action_wrapper<"erstkxsatlog"_n, &endorse_manage::erstkxsatlog>;
    
    using regvldtorlog_action = eosio::action_wrapper<"regvldtorlog"_n, &endorse_manage::regvldtorlog>;
    using endorse_action = eosio::action_wrapper<"endorse"_n, &endorse_manage::endorse>;

    static checksum256 compute_staking_id(const checksum160& proxy, const checksum160& staker, const name& validator) {
        vector<char> result;
        result.resize(48);
        eosio::datastream<char*> ds(result.data(), result.size());
        ds << proxy;
        ds << staker;
        ds << validator;
        return eosio::sha256((char*)result.data(), result.size());
    }

    static uint128_t compute_staking_id(const name& staker, const name& validator) {
        return uint128_t(staker.value) << 64 | validator.value;
    }

   private:
    global_id_table _global_id = global_id_table(_self, _self.value);
    validator_table _validator = validator_table(_self, _self.value);
    evm_staker_table _evm_stake = evm_staker_table(_self, _self.value);
    native_staker_table _native_stake = native_staker_table(_self, _self.value);
    credit_proxy_table _credit_proxy = credit_proxy_table(_self, _self.value);
    stat_table _stat = stat_table(_self, _self.value);
    config_table _config = config_table(_self, _self.value);

    uint64_t next_staking_id();

    void token_transfer(const name& from, const string& to, const extended_asset& value);

    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);

    void evm_claim(const name& caller, const checksum160& proxy, const checksum160& staker, const name& validator,
                   const uint16_t donate_rate);

    asset evm_stake_xsat_without_auth(const checksum160& proxy, const checksum160& staker, const name& validator,
                                      const asset& quantity);
    asset evm_unstake_xsat_without_auth(const checksum160& proxy, const checksum160& staker, const name& validator,
                                        const asset& quantity);
    asset stake_xsat_without_auth(const name& staker, const name& validator, const asset& quantity);

    asset unstake_xsat_without_auth(const name& staker, const name& validator, const asset& quantity);

    std::pair<asset, asset> evm_stake_without_auth(const checksum160& proxy, const checksum160& staker,
                                                   const name& validator, const asset& quantity,
                                                   const asset& qualification);
    std::pair<asset, asset> evm_unstake_without_auth(const checksum160& proxy, const checksum160& staker,
                                                     const name& validator, const asset& quantity,
                                                     const asset& qualification);
    std::pair<asset, asset> stake_without_auth(const name& staker, const name& validator, const asset& quantity,
                                               const asset& qualification);
    std::pair<asset, asset> unstake_without_auth(const name& staker, const name& validator, const asset& quantity,
                                                 const asset& qualification);

    template <typename T, typename C>
    void staking_change(validator_table::const_iterator& validator_itr, T& _stake, C& stake_itr, const asset& quantity,
                        const asset& qualification, const optional<uint8_t>& active_flag);

    template <typename T, typename C>
    void update_staking_reward(const uint128_t stake_acc_per_share, const uint128_t consensus_acc_per_share,
                               const uint64_t pre_stake, const uint64_t now_stake, T& _stake, C& stake_itr);

    void update_validator_reward(const uint64_t height, const name& validator, const uint64_t staking_reward,
                                 const uint64_t consensus_reward);

    void register_validator(const name& proxy, const name& validator, const string& financial_account,
                            const uint16_t commission_rate);

    asset get_qualification(const validator_row& validator_itr, const bool is_btc_validator, const checksum160& stake_addr);

#ifdef DEBUG
    template <typename T>
    void clear_table(T& table, uint64_t rows_to_clear);
#endif
};