#pragma once

#include "../internal/defines.hpp"
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <rwddist.xsat/rwddist.xsat.hpp>

using namespace eosio;
using namespace std;

/**
 * @brief GasFund Contract
 *
 * This contract is responsible for calculating and distributing rewards on the
 * blockchain, including regular block rewards and EVM gas fee rewards
 */
class [[eosio::contract("gasfund.xsat")]] gasfund : public contract {
public:
    using contract::contract;

    // CONSTANTS
    const std::set<name> RECEIVER_TYPES = {"validator"_n, "synchronizer"_n};

    // GASFUND Memo
    static constexpr string_view GASFUND_MEMO = "gasfund";
    /**
     * @brief Configuration structure, stores the last calculated block heights
     *
     * ### TABLE `config`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint16_t} enf_reward_rate` - EVM gas fee rate (5000 = 50%)
     * - `{uint16_t} rams_reward_rate` - RAMS gas fee rate (2500 = 25%)
     * - `{uint16_t} distribute_min_height_interval` - Min calculate interval (2 = 2 blocks)
     * - `{uint16_t} distribute_max_height_interval` - Max distribute height interval (16 = 16 blocks)
     * - `{uint64_t} start_distribute_height` - Start distribute height, not contain current height
     * - `{name} evm_fees_account` - EVM fees account (evmutil.xsat)
     * - `{checksum160} evm_proxy_address` - EVM proxy address
     * - `{string} enf_reward_address` - EVM reward address
     * - `{string} rams_reward_address` - RAMS reward address
     *
     * ### example
     *
     * ```json
     * {
     *   "enf_reward_rate": 5000,
     *   "rams_reward_rate": 2500,
     *   "distribute_min_height_interval": 1,
     *   "distribute_max_height_interval": 10,
     *   "start_distribute_height": 888888,
     *   "evm_fees_account": "evmutil.xsat",
     *   "evm_proxy_address": "0x1234567890abcdef1234567890abcdef1234567890",
     *   "enf_reward_address": "0x1234567890abcdef1234567890abcdef1234567890",
     *   "rams_reward_address": "receiver.rams"
     * }
     */
    struct [[eosio::table]] config_row {
        uint16_t enf_reward_rate = 5000;
        uint16_t rams_reward_rate = 2500;
        uint16_t distribute_min_height_interval = 1;
        uint16_t distribute_max_height_interval = 10;
        uint64_t start_distribute_height = 888888;
        name evm_fees_account = "evmutil.xsat"_n;

        checksum160 evm_proxy_address;
        string enf_reward_address;
        string rams_reward_address;
    };
    typedef eosio::singleton<"config"_n, config_row> config_table;

    /**
     * @brief Reward distribution record structure, storing details of the last
     * reward calculation
     *
     * ### TABLE `feestat`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} last_height` - Last block height for EOS gas fee calculation
     * - `{uint64_t} last_evm_height` - Last block height for EVM gas fee calculation
     * - `{asset} enf_unclaimed` - ENF unclaimed gas fees
     * - `{asset} rams_unclaimed` - RAMS unclaimed gas fees
     * - `{asset} consensus_unclaimed` - Consensus unclaimed gas fees ( sychronizer & validator )
     * - `{asset} total_enf_fees` - Total ENF gas fees
     * - `{asset} total_rams_fees` - Total RAMS gas fees
     * - `{asset} total_evm_gas_fees` - Total EVM gas fees
     * - `{asset} total_consensus_fees` - Total consensus fees
     *
     *
     */
    struct [[eosio::table]] fees_stat_row {
        uint64_t last_height;
        uint64_t last_evm_height;

        asset enf_unclaimed;
        asset rams_unclaimed;
        asset consensus_unclaimed;

        asset total_enf_fees;
        asset total_rams_fees;
        asset total_evm_gas_fees;
        asset total_consensus_fees;
    };

    typedef eosio::singleton<"feestat"_n, fees_stat_row> fees_stat_table;

    /**
     * @brief Consensus fees information
     *
     * ### TABLE `consensusfee`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} id` - Consensus fee ID
     * - `{name} receiver` - Receiver account ( validator or synchronizer eos account)
     * - `{name} receiver_type` - Receiver type (validator or synchronizer)
     * - `{asset} unclaimed` - Unclaimed reward amount
     * - `{asset} total_claimed` - Total claimed reward amount
     * - `{time_point_sec} last_claim_time` - Last claim time
     *
     *  total_reward = total_claimed + unclaimed
     */
    struct [[eosio::table]] consensus_fee_row {
        uint64_t id;
        name receiver;
        name receiver_type;
        asset unclaimed;
        asset total_claimed;
        time_point_sec last_claim_time;

        uint64_t get_receiver_id() const {
            return receiver_type == "validator"_n ? receiver.value : receiver.value | XSAT_SCOPE_MASK;
        }

        uint64_t primary_key() const {
            return id;
        }

        uint64_t by_receiver() const {
            return get_receiver_id();
        }
    };
    typedef eosio::multi_index<
        "consensusfee"_n, consensus_fee_row,
        indexed_by<"byreceiver"_n, const_mem_fun<consensus_fee_row, uint64_t, &consensus_fee_row::by_receiver>>>
        consensus_fees_table;

    /**
     * @brief fee transfer distribution record
     *
     * Records the distribution details of each EVM fee transfer
     * Uses start_height as primary key to track each distribute
     *
     * ### TABLE `distribute`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} start_height` - Start height ( last transfer evm gas fee height )
     * - `{uint64_t} end_height` - End height ( current transfer evm gas fee height )
     * - `{asset} total_fees` - Total fees (between start_height and end_height total evm gas fee )
     * - `{asset} enf_fees` - ENF fees (between start_height and end_height enf evm gas fee )
     * - `{asset} rams_fees` - RAMS fees (between start_height and end_height rams evm gas fee )
     * - `{asset} consensus_fees` - Consensus fees (between start_height and end_height consensus evm gas fee )
     * - `{asset} total_xsat_rewards` - Total XSAT rewards (between start_height and end_height total xsat rewards )
     * - `{time_point_sec} distribute_time` - Distribute time
     *
     */
    struct [[eosio::table]] distribute_row {
        uint64_t start_height;          
        uint64_t end_height;           
        asset total_fees;              
        asset enf_fees;                
        asset rams_fees;               
        asset consensus_fees;          
        asset total_xsat_rewards;      
        time_point_sec distribute_time; 

        uint64_t primary_key() const {
            return start_height;
        }
    };
    typedef eosio::multi_index<"distributes"_n, distribute_row> distribute_table;

    /**
     * @brief Gas fee distribution record
     *
     * Stores the distribution details of gas fees for each receiver
     *
     * ### TABLE `distdetails`
     *
     * ### scope `start_height from distribute table`
     * ### params
     *
     * - `{uint64_t} id` - Distribute detail ID
     * - `{name} receiver` - Receiver account
     * - `{name} receiver_type` - Receiver type (validator or synchronizer)
     * - `{asset} reward` - total xsat rewards between start_height and end_height
     *
     */
    struct [[eosio::table]] distribute_detail_row {
        uint64_t id;
        name receiver;      
        name receiver_type; 
        asset reward;       

        uint64_t get_receiver_id() const {
            return receiver_type == "validator"_n ? receiver.value : receiver.value | XSAT_SCOPE_MASK;
        }
        uint64_t primary_key() const {
            return id;
        }
        uint64_t by_receiver() const {
            return get_receiver_id();
        }
    };
    typedef eosio::multi_index<
        "distdetails"_n, distribute_detail_row,
        indexed_by<"byreceiver"_n, const_mem_fun<distribute_detail_row, uint64_t, &distribute_detail_row::by_receiver>>>
        distribute_detail_table;

    // handle and distribute evm gas fee transfer
    [[eosio::on_notify("*::transfer")]]
    void on_transfer(const name& from, const name& to, const asset& quantity, const string& memo);

    // calculate and distribute EOS gas fee
    [[eosio::action]]
    void distribute();

    // claim gas fee for validator and synchronizer
    [[eosio::action]]
    void claim(const name& receiver, const uint8_t receiver_type);

    // claim gas fee for ENF
    [[eosio::action]]
    void enfclaim();

    // claim gas fee for RAMS
    [[eosio::action]]
    void ramsclaim();

    // claim gas fee for validator and synchronizer from evm
    [[eosio::action]]
    void evmclaim(const name& caller, const checksum160& proxy_address, const checksum160& sender, const name& receiver,
                  const uint8_t receiver_type);

    // claim gas fee for ENF from evm
    [[eosio::action]]
    void evmenfclaim(const name& caller, const checksum160& proxy_address, const checksum160& sender);

    // claim gas fee for RAMS from evm
    [[eosio::action]]
    void evmramsclaim(const name& caller, const checksum160& proxy_address, const checksum160& sender);

    /**
     * @brief Set configuration
     *
     * @param config Configuration
     */
    [[eosio::action]]
    void config(const config_row& config);

#ifdef DEBUG
    [[eosio::action]]
    void cleartable(const name& table) {
        require_auth(get_self());
        if (table == "consensusfee"_n) {
            auto _begin = _consensus_fees.begin();
            auto _end = _consensus_fees.end();
            while (_begin != _end) {
                _begin = _consensus_fees.erase(_begin);
            }
        } else if (table == "distribute"_n) {
            auto _begin = _distributes.begin();
            auto _end = _distributes.end();
            while (_begin != _end) {
                _begin = _distributes.erase(_begin);
            }
        } else if (table == "feestat"_n) {
            _fees_stat.remove();
        } else if (table == "config"_n) {
            _config.remove();
        }
    };
#endif

    // Log action
    [[eosio::action]]
    void configlog(const config_row& config) {
        require_auth(get_self());
    };

    [[eosio::action]]
    void distributlog(const distribute_row& distribute) {
        require_auth(get_self());
    };

    [[eosio::action]]
    void claimlog(const name& sender, const name& receiver, const uint8_t receiver_type, const asset& quantity) {
        require_auth(get_self());
    };

    [[eosio::action]]
    void enfclaimlog(const name& sender, const string& address, const asset& quantity) {
        require_auth(get_self());
    };

    [[eosio::action]]
    void ramsclaimlog(const name& sender, const string& address, const asset& quantity) {
        require_auth(get_self());
    };

    [[eosio::action]]
    void evmclaimlog(const name& caller, const checksum160& proxy_address, const checksum160& sender,
                     const name& receiver, const uint8_t receiver_type, const asset& quantity) {
        require_auth(get_self());
    };

    [[eosio::action]]
    void evmenfclog(const name& caller, const checksum160& proxy_address, const checksum160& sender,
                    const asset& quantity) {
        require_auth(get_self());
    };

    [[eosio::action]]
    void evmramsclog(const name& caller, const checksum160& proxy_address, const checksum160& sender,
                     const asset& quantity) {
        require_auth(get_self());
    };

    using config_action = eosio::action_wrapper<"config"_n, &gasfund::config>;
    using distribute_action = eosio::action_wrapper<"distribute"_n, &gasfund::distribute>;
    using claim_action = eosio::action_wrapper<"claim"_n, &gasfund::claim>;
    using evmclaim_action = eosio::action_wrapper<"evmclaim"_n, &gasfund::evmclaim>;
    using evmenfclaim_action = eosio::action_wrapper<"evmenfclaim"_n, &gasfund::evmenfclaim>;
    using evmramsclaim_action = eosio::action_wrapper<"evmramsclaim"_n, &gasfund::evmramsclaim>;

    // log action
    using configlog_action = eosio::action_wrapper<"configlog"_n, &gasfund::configlog>;
    using distributlog_action = eosio::action_wrapper<"distributlog"_n, &gasfund::distributlog>;
    using claimlog_action = eosio::action_wrapper<"claimlog"_n, &gasfund::claimlog>;
    using enfclaimlog_action = eosio::action_wrapper<"enfclaimlog"_n, &gasfund::enfclaimlog>;
    using ramsclaimlog_action = eosio::action_wrapper<"ramsclaimlog"_n, &gasfund::ramsclaimlog>;
    using evmclaimlog_action = eosio::action_wrapper<"evmclaimlog"_n, &gasfund::evmclaimlog>;
    using evmenfclog_action = eosio::action_wrapper<"evmenfclog"_n, &gasfund::evmenfclog>;
    using evmramsclog_action = eosio::action_wrapper<"evmramsclog"_n, &gasfund::evmramsclog>;

private:
    /**
     * @brief Fee distribution result structure
     *
     * Used to accumulate fee distribution results before batch updating the
     * database
     */
    struct reward_distribution_result {
        name receiver;
        uint64_t reward = 0;
        bool is_synchronizer = false;
    };

    // Return structure containing distribution results and total rewards
    struct reward_calculation_result {
        vector<reward_distribution_result> distributions;
        uint64_t total_rewards = 0;
    };

    // Configuration singleton table
    config_table _config = config_table(_self, _self.value);
    fees_stat_table _fees_stat = fees_stat_table(_self, _self.value);
    consensus_fees_table _consensus_fees = consensus_fees_table(_self, _self.value);
    distribute_table _distributes = distribute_table(_self, _self.value);
    distribute_table _evm_distributes = distribute_table(_self, EVM_UTIL_CONTRACT.value);

    void handle_evm_fees_transfer(const name& from, const name& to, const asset& quantity, const string& memo);
    asset receiver_claim(const name& receiver, const uint8_t receiver_type);

    // Distribution helper functions - optimized parameter passing for basic types
    reward_calculation_result calculate_reward(uint64_t height);

    vector<reward_distribution_result>
    calculate_btc_validator_rewards(const reward_distribution::reward_log_table::const_iterator& _reward_log_itr,
                                    uint64_t staking_reward);

    vector<reward_distribution_result>
    calculate_xsat_validator_rewards(const reward_distribution::reward_log_table::const_iterator& _xsat_reward_log_itr,
                                     uint64_t consensus_reward);

    asset enfclaim_without_auth(const config_row& config);
    asset ramsclaim_without_auth(const config_row& config);

    uint64_t safe_pct(uint64_t value, uint64_t numerator, uint64_t denominator);
    void token_transfer(const name& from, const string& to, const extended_asset& value);
    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);
};
