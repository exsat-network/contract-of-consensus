#include <blkendt.xsat/blkendt.xsat.hpp>
#include <exsat.xsat/exsat.xsat.hpp>
#include <rwddist.xsat/rwddist.xsat.hpp>
#include <utxomng.xsat/utxomng.xsat.hpp>

#ifdef DEBUG
#include "./src/debug.hpp"
#endif

[[eosio::action]]
void reward_distribution::setrwdconfig(reward_config_row config) {
    require_auth(get_self());

    reward_config_table _reward_config(get_self(), get_self().value);
    _reward_config.set(config, get_self());
}

endorse_manage::consensus_config_row _get_consensus_config() {
    // consensus config
    endorse_manage::consensus_config_table _consensus_config
        = endorse_manage::consensus_config_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    return _consensus_config.get_or_default();
}

reward_distribution::reward_rate_t reward_distribution::get_reward_rate(int version) {
    reward_config_table _reward_config(get_self(), get_self().value);
    reward_config_row reward_config{};
    if (_reward_config.exists()) {
        reward_config = _reward_config.get();
    }
    reward_config.cached_version = version;
    _reward_config.set(reward_config, get_self());

    if (reward_config.cached_version <= 1)
        return reward_config.v1;
    else {
        return reward_config.v2;
    }
}

//@auth utxomng.xsat
[[eosio::action]]
void reward_distribution::distribute(const uint64_t height) {
    require_auth(UTXO_MANAGE_CONTRACT);

    endorse_manage::consensus_config_row consensus_config = _get_consensus_config();
    reward_rate_t reward_rate = get_reward_rate(consensus_config.version);
    bool enable_exsat_consensus_reward = (consensus_config.flags & consensus_config.xsat_consensus_mask);

    check(_btc_reward_log.find(height) == _btc_reward_log.end(),
          "rwddist.xsat::distribute: the current block has been allocated rewards");

    check(_xsat_reward_log.find(height) == _xsat_reward_log.end(),
          "rwddist.xsat::distribute: the current block has been allocated rewards(2)");

    utxo_manage::chain_state_table _chain_state(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto chain_state = _chain_state.get();
    check(chain_state.migrating_height == height, "rwddist.xsat::distribute: no rewards are being distributed");

    auto hash = chain_state.migrating_hash;

    // calc reward
    uint64_t halvings = (height - START_HEIGHT) / SUBSIDY_HALVING_INTERVAL;
    int64_t nSubsidy = halvings >= 64 ? 0 : XSAT_REWARD_PER_BLOCK >> halvings;

    int64_t synchronizer_rewards = 0;
    int64_t btc_consensus_rewards = 0;
    int64_t xsat_consensus_rewards = 0;
    int64_t btc_staking_rewards = 0;
    if (nSubsidy > 0) {
        // issue
        exsat::issue_action _issue(EXSAT_CONTRACT, {get_self(), "active"_n});
        _issue.send(get_self(), asset{nSubsidy, XSAT_SYMBOL}, "block reward");

        //  synchronizer reward
        uint64_t synchronizer_rate = 0;
        if (chain_state.miner == chain_state.synchronizer) {
            synchronizer_rate = reward_rate.miner_reward_rate;
        } else {
            synchronizer_rate = reward_rate.synchronizer_reward_rate;
        }
        synchronizer_rewards = uint128_t(nSubsidy) * synchronizer_rate / RATE_BASE;

        //  consensus reward (for btc staking validators)
        btc_consensus_rewards = uint128_t(nSubsidy) * reward_rate.btc_consensus_reward_rate / RATE_BASE;

        if (enable_exsat_consensus_reward) {
            //  xsat consensus reward (for xsat staking validators)
            xsat_consensus_rewards = uint128_t(nSubsidy) * reward_rate.xsat_consensus_reward_rate / RATE_BASE;
        }

        // staking reward (for btc staking validators)
        btc_staking_rewards = nSubsidy - synchronizer_rewards - btc_consensus_rewards - xsat_consensus_rewards;
    }

    // to BTC validators
    distribute_per_symbol(chain_state, height, true, synchronizer_rewards, btc_staking_rewards, btc_consensus_rewards,
                          _btc_reward_log, _btc_reward_balance);

    if (enable_exsat_consensus_reward && xsat_consensus_rewards > 0) {
        // to XSAT validators
        distribute_per_symbol(chain_state, height, false, 0, 0, xsat_consensus_rewards, _xsat_reward_log, _xsat_reward_balance);
    }
}

template <typename ChainStateRow>
void reward_distribution::distribute_per_symbol(const ChainStateRow& chain_state, const uint64_t height, bool is_btc,
                                                int64_t synchronizer_rewards, int64_t staking_rewards, int64_t consensus_rewards,
                                                reward_log_table& _reward_log, reward_balance_table& _reward_balance) {
    auto hash = chain_state.migrating_hash;

    // FIXME: height lookup
    block_endorse::endorsement_table _endorsement(
        BLOCK_ENDORSE_CONTRACT, (is_btc ? height : height | 0x100000000)); // blkendt.xsat
    auto endorsement_idx = _endorsement.get_index<"byhash"_n>();
    auto endorsement_itr = endorsement_idx.find(hash);

    if (!is_btc && endorsement_itr == endorsement_idx.end()) {
        return; // no endorsement for xsat validators
    }

    auto num_reached_consensus = endorsement_itr->num_reached_consensus();
    auto num_validators = endorsement_itr->num_validators();

    uint64_t endorsed_staking = 0;
    uint64_t reached_consensus_staking = 0;
    vector<validator_info> provider_validators;
    provider_validators.reserve(endorsement_itr->provider_validators.size());
    for (size_t i = 0; i < endorsement_itr->provider_validators.size(); ++i) {
        const auto validator = endorsement_itr->provider_validators[i];
        provider_validators.emplace_back(
            validator_info{.account = validator.account, .staking = validator.staking, .created_at = validator.created_at});

        endorsed_staking += validator.staking;
        if (i < num_reached_consensus || !is_btc) {
            reached_consensus_staking += validator.staking;
        }
    }

    auto reward_log_itr = _reward_log.emplace(get_self(), [&](auto& row) {
        row.height = height;
        row.hash = hash;
        row.synchronizer_rewards = {synchronizer_rewards, XSAT_SYMBOL};
        row.consensus_rewards = {consensus_rewards, XSAT_SYMBOL};
        row.staking_rewards = {staking_rewards, XSAT_SYMBOL};
        row.provider_validators = provider_validators;
        row.endorsed_staking = endorsed_staking;
        row.reached_consensus_staking = reached_consensus_staking;
        row.miner = chain_state.miner;
        row.synchronizer = chain_state.synchronizer;
        row.parser = chain_state.parser;
        row.num_validators = num_validators;
    });

    auto reward_balance = _reward_balance.get_or_default();
    reward_balance.height = height;
    reward_balance.synchronizer_rewards_unclaimed = reward_log_itr->synchronizer_rewards;
    reward_balance.consensus_rewards_unclaimed = reward_log_itr->consensus_rewards;
    reward_balance.staking_rewards_unclaimed = reward_log_itr->staking_rewards;
    _reward_balance.set(reward_balance, get_self());
}

[[eosio::action]]
void reward_distribution::endtreward(const uint64_t height, uint32_t from_index, const uint32_t to_index) {
    require_auth(UTXO_MANAGE_CONTRACT);
    endtreward_per_symbol(height, from_index, to_index, true, _btc_reward_log, _btc_reward_balance);

    // XSAT consensus reward
    // TODO:  
    auto itr = _xsat_reward_log.find(height);
    if (itr != _xsat_reward_log.end()) {
        endtreward_per_symbol(height, from_index, to_index, false, _xsat_reward_log, _xsat_reward_balance);
    }
}

[[eosio::action]]
void reward_distribution::endtreward2(const uint64_t height, uint32_t from_index, const uint32_t to_index) {
    require_auth(UTXO_MANAGE_CONTRACT);
    endtreward_per_symbol(height, from_index, to_index, false, _xsat_reward_log, _xsat_reward_balance);
}

void reward_distribution::endtreward_per_symbol(const uint64_t height, uint32_t from_index, const uint32_t to_index,
                                                bool is_btc_validators, reward_log_table& _reward_log,
                                                reward_balance_table& _reward_balance) {
    auto reward_log_itr = _reward_log.require_find(height, "rwddist.xsat::endtreward: no rewards are being distributed");

    check(reward_log_itr->num_validators_assigned < reward_log_itr->provider_validators.size(),
          "rwddist.xsat::endtreward: the current block has distributed rewards");
    check(from_index == reward_log_itr->num_validators_assigned, "rwddist.xsat::endtreward: invalid from_index");
    check(to_index > from_index && to_index <= reward_log_itr->provider_validators.size(),
          "rwddist.xsat::endtreward: invalid to_index");

    auto num_reached_consensus = xsat::utils::num_reached_consensus(reward_log_itr->num_validators);

    vector<endorse_manage::reward_details_row> reward_details;
    reward_details.reserve(to_index - from_index);

    auto reward_balance = _reward_balance.get_or_default();
    asset total_rewards = {0, reward_log_itr->staking_rewards.symbol};

    // Calculate consensus rewards by dividing total consensus rewards evenly among all validators
    int64_t consensus_reward_amount
        = uint128_t(reward_log_itr->consensus_rewards.amount) / reward_log_itr->provider_validators.size();

    for (; from_index < to_index; from_index++) {
        auto validator = reward_log_itr->provider_validators[from_index];
        // endorse / consensus staking
        auto endorse_staking = validator.staking;
        auto consensus_staking = (!is_btc_validators || num_reached_consensus > from_index) ? validator.staking : 0;

        // calculate the number of rewards
        int64_t staking_reward_amount = 0;

        // The last one to distribute the remaining rewards
        if (from_index != reward_log_itr->provider_validators.size() - 1) {

            staking_reward_amount
                = uint128_t(reward_log_itr->staking_rewards.amount) * endorse_staking / reward_log_itr->endorsed_staking;
        } else {

            staking_reward_amount = reward_balance.staking_rewards_unclaimed.amount;
        }

        reward_details.emplace_back(endorse_manage::reward_details_row{
            .validator = validator.account,
            .staking_rewards = {staking_reward_amount, reward_log_itr->staking_rewards.symbol},
            .consensus_rewards = {consensus_reward_amount, reward_log_itr->consensus_rewards.symbol}});

        total_rewards.amount += staking_reward_amount + consensus_reward_amount;

        reward_balance.staking_rewards_unclaimed.amount -= staking_reward_amount;
        reward_balance.consensus_rewards_unclaimed.amount -= consensus_reward_amount;
    }

    // transfer to endrmng.xsat
    token_transfer(get_self(), ENDORSER_MANAGE_CONTRACT, {total_rewards, EXSAT_CONTRACT}, "consensus rewards");

    // distribute
    endorse_manage::distribute_action _distribute(ENDORSER_MANAGE_CONTRACT, {get_self(), "active"_n});
    _distribute.send(height, reward_details);

    // log
    reward_distribution::endtrwdlog_action _endtrwdlog(get_self(), {get_self(), "active"_n});
    _endtrwdlog.send(height, reward_log_itr->hash, reward_details);

    if (to_index == reward_log_itr->provider_validators.size()) {
        // transfer to poolreg.xsat
        if (reward_log_itr->synchronizer_rewards.amount > 0) {
            token_transfer(get_self(), POOL_REGISTER_CONTRACT, {reward_log_itr->synchronizer_rewards, EXSAT_CONTRACT},
                           reward_log_itr->parser.to_string() + "," + std::to_string(height));
        }

        // log
        reward_distribution::rewardlog_action _rewardlog(get_self(), {get_self(), "active"_n});
        _rewardlog.send(height, reward_log_itr->hash, reward_log_itr->synchronizer, reward_log_itr->miner, reward_log_itr->parser,
                        reward_log_itr->synchronizer_rewards, reward_log_itr->staking_rewards, reward_log_itr->consensus_rewards);

        reward_balance.synchronizer_rewards_unclaimed.amount = 0;
    }

    _reward_balance.set(reward_balance, get_self());

    _reward_log.modify(reward_log_itr, same_payer, [&](auto& row) {
        row.num_validators_assigned = to_index;
        row.latest_exec_time = current_time_point();
#ifndef UNITTEST
        row.tx_id = xsat::utils::get_trx_id();
#endif
    });
}

void reward_distribution::token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo) {
    exsat::transfer_action transfer(value.contract, {from, "active"_n});
    transfer.send(from, to, value.quantity, memo);
}