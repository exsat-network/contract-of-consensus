#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include "../internal/defines.hpp"
#include <endrmng.xsat/endrmng.xsat.hpp>

using namespace eosio;
using namespace std;

class [[eosio::contract("rwddist.xsat")]] reward_distribution : public contract {
   public:
    using contract::contract;

    /**
     * ## STRUCT `validator_info`
     *
     * - `{name} account` - validator account
     * - `{uint64_t} staking` - the validator's staking amount
     * - `{time_point_sec} created_at` - created at time
     *
     * ### example
     *
     * ```json
     * {
     *   "account": "test.xsat",
     *   "staking": "10200000000",
     *   "created_at": "2024-08-13T00:00:00"
     * }
     * ```
     */
    struct validator_info {
        name account;
        uint64_t staking;
        time_point_sec created_at;
    };

    /**
     * ## TABLE `rewardlogs`
     *
     * ### scope `height`
     * ### params
     *
     * - `{uint64_t} height` - block height
     * - `{checksum256} hash` - block hash
     * - `{asset} synchronizer_rewards` - the synchronizer assigns the number of rewards
     * - `{asset} consensus_rewards` - the consensus validator allocates the number of rewards
     * - `{asset} staking_rewards` - the validator assigns the number of rewards
     * - `{uint32_t} num_validators` - the number of validators who pledge more than 100 BTC
     * - `{std::vector<validator_info>} provider_validators` - list of endorsed validators
     * - `{uint64_t} endorsed_staking` - total endorsed staking amount
     * - `{uint64_t} reached_consensus_staking` - the total staking amount to reach consensus is `(number of validators * 2/3+ 1 staking amount)`
     * - `{uint32_t} num_validators_assigned` - the number of validators that have been allocated rewards
     * - `{name} synchronizer` -synchronizer account
     * - `{name} miner` - miner account
     * - `{name} parser` - parse the account of the block
     * - `{checksum256} tx_id` - tx_id of the reward after distribution
     * - `{time_point_sec} latest_exec_time` - latest reward distribution time
     *
     * ### example
     *
     * ```json
     * {
     *   "height": 840000,
     *   "hash": "0000000000000000000320283a032748cef8227873ff4872689bf23f1cda83a5",
     *   "synchronizer_rewards": "5.00000000 XSAT",
     *   "consensus_rewards": "5.00000000 XSAT",
     *   "staking_rewards": "40.00000000 XSAT",
     *   "num_validators": 2,
     *   "provider_validators": [{
     *       "account": "alice",
     *       "staking": "10010000000"
     *       },{
     *       "account": "bob",
     *       "staking": "10200000000"
     *       }
     *   ],
     *   "endorsed_staking": "20210000000",
     *   "reached_consensus_staking": "20210000000",
     *   "num_validators_assigned": 2,
     *   "synchronizer": "alice",
     *   "miner": "",
     *   "parser": "alice",
     *   "tx_id": "0000000000000000000000000000000000000000000000000000000000000000",
     *   "latest_exec_time": "2024-07-13T09:06:56"
     * }
     * ```
     */
    struct [[eosio::table]] reward_log_row {
        uint64_t height;
        checksum256 hash;
        asset synchronizer_rewards;
        asset consensus_rewards;
        asset staking_rewards;
        uint32_t num_validators;
        std::vector<validator_info> provider_validators;
        uint64_t endorsed_staking;
        uint64_t reached_consensus_staking;
        uint32_t num_validators_assigned;
        name synchronizer;
        name miner;
        name parser;
        checksum256 tx_id;
        time_point_sec latest_exec_time;
        uint64_t primary_key() const { return height; }
        uint64_t by_synchronizer() const { return synchronizer.value; }
        uint64_t by_parser() const { return parser.value; }
        uint64_t by_miner() const { return miner.value; }
    };
    typedef eosio::multi_index<
        "rewardlogs"_n, reward_log_row,
        eosio::indexed_by<"bysyncer"_n, const_mem_fun<reward_log_row, uint64_t, &reward_log_row::by_synchronizer>>,
        eosio::indexed_by<"byparser"_n, const_mem_fun<reward_log_row, uint64_t, &reward_log_row::by_parser>>,
        eosio::indexed_by<"byminer"_n, const_mem_fun<reward_log_row, uint64_t, &reward_log_row::by_miner>>>
        reward_log_table;

    /**
     * ## TABLE `rewardbal`
     *
     * ### scope `get_self()`
     * ### params
     *
     * - `{uint64_t} height` - block height
     * - `{asset} synchronizer_rewards_unclaimed` - unclaimed synchronizer rewards
     * - `{asset} consensus_rewards_unclaimed` - unclaimed consensus rewards
     * - `{asset} staking_rewards_unclaimed` - unclaimed staking rewards
     *
     * ### example
     *
     * ```json
     * {
     *   "height": 840000,
     *   "synchronizer_rewards_unclaimed": "5.00000000 XSAT",
     *   "consensus_rewards_unclaimed": "5.00000000 XSAT",
     *   "staking_rewards_unclaimed": "40.00000000 XSAT"
     * }
     * ```
     */
    struct [[eosio::table]] reward_balance_row {
        uint64_t height = 0;
        asset synchronizer_rewards_unclaimed = {0, XSAT_SYMBOL};
        asset consensus_rewards_unclaimed = {0, XSAT_SYMBOL};
        asset staking_rewards_unclaimed = {0, XSAT_SYMBOL};
    };
    typedef eosio::singleton<"rewardbal"_n, reward_balance_row> reward_balance_table;


    struct reward_rate_t {
        uint64_t miner_reward_rate = 0;
        uint64_t synchronizer_reward_rate = 0;
        uint64_t btc_consensus_reward_rate = 0;
        uint64_t xsat_consensus_reward_rate = 0;
        uint64_t xsat_staking_reward_rate = 0;
        uint64_t reserve1 = 0; // for future use
        uint64_t reserve2 = 0; // for future use
        std::optional<int> reserved3 = {}; // for future use
    };
    struct [[eosio::table]] reward_config_row {
        uint16_t cached_version = 0; // 0 - unset
        reward_rate_t v1 = {
            .miner_reward_rate = MINER_REWARD_RATE, 
            .synchronizer_reward_rate = SYNCHRONIZER_REWARD_RATE, 
            .btc_consensus_reward_rate = CONSENSUS_REWARD_RATE,
        };
        reward_rate_t v2 = {
            .miner_reward_rate = RATE_BASE / 5, /*20%*/
            .synchronizer_reward_rate = RATE_BASE / 20, /* 5% */
            .btc_consensus_reward_rate = 0,
            .xsat_consensus_reward_rate = RATE_BASE / 20, /* 5% */
        };
    };
    typedef eosio::singleton<"rewardconfig"_n, reward_config_row> reward_config_table;

    /**
     * ## ACTION `distribute`
     *
     * - **authority**: `utxomng.xsat`
     *
     * > Allocate rewards and record allocation information.
     *
     * ### params
     *
     * - `{uint64_t} height` - Block height for allocating rewards
     *
     * ### example
     *
     * ```bash
     * $ cleos push action rwddist.xsat distribute '[840000]' -p utxomng.xsat
     * ```
     */
    [[eosio::action]]
    void distribute(const uint64_t height);

    // ChainStateRow = utxo_manage::chain_state_row
    template <typename ChainStateRow>
    void distribute_per_symbol(const ChainStateRow &chain_state,
        const uint64_t height, bool is_btc, 
        int64_t synchronizer_rewards,
        int64_t staking_rewards,
        int64_t consensus_rewards, 
        reward_log_table &_reward_log, 
        reward_balance_table &_reward_balance);

    /**
     * ## ACTION `endtreward`
     *
     * - **authority**: `utxomng.xsat`
     *
     * > Allocate rewards and record allocation information.
     *
     * ### params
     *
     * - `{uint64_t} height` - block height
     * - `{uint32_t} from_index` - the starting reward index of provider_validators
     * - `{uint32_t} to_index` - end reward index of provider_validators
     *
     * ### example
     *
     * ```bash
     * $ cleos push action rwddist.xsat endtreward '[840000, 0, 10]' -p utxomng.xsat
     * ```
     */
    [[eosio::action]]
    void endtreward(const uint64_t height, const uint32_t from_index, const uint32_t to_index);

    [[eosio::action]]
    void endtreward2(const uint64_t height, const uint32_t from_index, const uint32_t to_index);

    void endtreward_per_symbol(const uint64_t height, uint32_t from_index, const uint32_t to_index, 
        bool is_btc_validators, reward_log_table &_reward_log, reward_balance_table &_reward_balance);

#ifdef DEBUG
    [[eosio::action]]
    void cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows);

    [[eosio::action]]
    void setstate(uint64_t height, name miner, name synchronizer, std::vector<name> provider_validators, uint64_t staking);
#endif

    // logs
    [[eosio::action]]
    void rewardlog(const uint64_t height, const checksum256& hash, const name& synchronizer, const name& miner,
                   const name& parser, const asset& synchronizer_reward, const asset& staking_reward,
                   const asset& consensus_reward) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void endtrwdlog(const uint64_t height, const checksum256& hash,
                    const vector<endorse_manage::reward_details_row>& reward_details) {
        require_auth(get_self());
    }

    [[eosio::action]]
    void setrwdconfig(reward_config_row config);

    using distribute_action = eosio::action_wrapper<"distribute"_n, &reward_distribution::distribute>;
    using endtreward_action = eosio::action_wrapper<"endtreward"_n, &reward_distribution::endtreward>;
    using rewardlog_action = eosio::action_wrapper<"rewardlog"_n, &reward_distribution::rewardlog>;
    using endtrwdlog_action = eosio::action_wrapper<"endtrwdlog"_n, &reward_distribution::endtrwdlog>;

   private:
    // init table
    reward_log_table _btc_reward_log = reward_log_table(_self, _self.value);
    reward_log_table _xsat_reward_log = reward_log_table(_self, "xsat"_n.value);

    reward_balance_table _btc_reward_balance = reward_balance_table(_self, _self.value);
    reward_balance_table _xsat_reward_balance = reward_balance_table(_self, "xsat"_n.value);

    void token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo);

    reward_rate_t get_reward_rate(int version);

#ifdef DEBUG
    template <typename T>
    void clear_table(T& table, uint64_t rows_to_clear);
#endif
};