#include "gasfund.xsat.hpp"
#include <eosio/system.hpp>
#include <btc.xsat/btc.xsat.hpp>
#include <rescmng.xsat/rescmng.xsat.hpp>
#include <utxomng.xsat/utxomng.xsat.hpp>
#include "../internal/defines.hpp"
#include "../internal/safemath.hpp"
#include "../internal/utils.hpp"

/**
 * @brief Safe percentage calculation function to prevent overflow
 *
 * Uses 128-bit arithmetic to safely calculate value * numerator / denominator
 * Checks for division by zero and ensures calculations won't overflow
 *
 * @param value The base value to calculate percentage of
 * @param numerator The numerator of the percentage
 * @param denominator The denominator of the percentage
 * @return uint64_t The calculated result
 */
uint64_t gasfund::safe_pct(uint64_t value, uint64_t numerator, uint64_t denominator) {
    check(denominator > 0, "gasfund.xsat::safe_pct: Division by zero");

    // Use 128-bit arithmetic to prevent overflow
    return safemath128::muldiv(value, numerator, denominator);
}

[[eosio::action]]
void gasfund::config(const config_row& config) {
    require_auth(get_self());
    _config.set(config, get_self());
}

[[eosio::action]]
void gasfund::claim(const name& receiver, const uint8_t receiver_type) {
    require_auth(receiver);

    // send log
    claimlog(receiver, receiver_type, receiver_claim(receiver, receiver_type));
}

// receiver_type: 0 for validator, 1 for synchronizer
[[eosio::action]]
void gasfund::evmclaim(const name& caller, const checksum160& proxy_address, const checksum160& sender, const name& receiver,
                       const uint8_t receiver_type) {
    require_auth(caller);

    auto config = _config.get();
    check(config.evm_proxy_address != checksum160(), "gasfund.xsat::evmclaim: proxy address is not set");
    check(proxy_address == config.evm_proxy_address, "gasfund.xsat::evmclaim: invalid proxy address");

    // send log
    evmclaimlog(caller, proxy_address, sender, receiver, receiver_type, receiver_claim(receiver, receiver_type));
}

asset gasfund::receiver_claim(const name& receiver, const uint8_t receiver_type) {

    auto _consensus_fees_index = _consensus_fees.get_index<"byreceiver"_n>();
    auto scope = receiver_type == 0 ? receiver.value : receiver.value | XSAT_SCOPE_MASK;
    auto _consensus_fees_itr
        = _consensus_fees_index.require_find(scope, "gasfund.xsat::receiver_claim: consensus fees not found");
    check(_consensus_fees_itr->unclaimed.amount > 0, "gasfund.xsat::receiver_claim: no unclaimed reward");

    // send to validator reward address
    endorse_manage::validator_table _validator
        = endorse_manage::validator_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto validator_itr = _validator.require_find(receiver.value, "gasfund.xsat::receiver_claim: validator not found");

    auto _feestat_itr = _fees_stat.get_or_default();
    auto unclaimed = _consensus_fees_itr->unclaimed;
    check(_feestat_itr.consensus_unclaimed.amount >= unclaimed.amount,
          "gasfund.xsat::receiver_claim: consensus unclaimed is less than receiver unclaimed");

    _consensus_fees_index.modify(_consensus_fees_itr, get_self(), [&](auto& row) {
        row.unclaimed = asset(0, unclaimed.symbol);
        row.total_claimed = asset(safemath::add(row.total_claimed.amount, unclaimed.amount), unclaimed.symbol);
        row.last_claim_time = current_time_point();
    });

    auto _feestat = _fees_stat.get_or_default();
    _feestat.consensus_unclaimed = asset(safemath::sub(_feestat.consensus_unclaimed.amount, unclaimed.amount), unclaimed.symbol);
    _fees_stat.set(_feestat, get_self());

    return unclaimed;
}

[[eosio::action]]
void gasfund::distribute() {
    require_auth(get_self());

    auto config = _config.get_or_default();
    // chain state
    utxo_manage::chain_state_table _chain_state(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto chain_state = _chain_state.get();
    check(chain_state.irreversible_height >= config.start_distribute_height,
          "gasfund.xsat::distribute: current height is less than start distribute height");

    auto _feestat = _fees_stat.get_or_default();
    auto start_height = _feestat.last_height > 0 ? _feestat.last_height + 1 : config.start_distribute_height;
    auto end_height = chain_state.irreversible_height;
    auto min_height = start_height + config.distribute_min_height_interval;
    auto max_height = start_height + config.distribute_max_height_interval;
    check(end_height >= min_height,
          "gasfund.xsat::distribute: height must be greater than or equal to " + std::to_string(min_height));

    // if end_height is greater than max_height, set end_height to max_height
    if (end_height > max_height) {
        end_height = max_height;
    }

    resource_management::feestat_table _resc_feestat(RESOURCE_MANAGE_CONTRACT, RESOURCE_MANAGE_CONTRACT.value);
    uint64_t total_consensus_fees = 0;
    uint64_t total_enf_fees = 0;
    uint64_t total_rams_fees = 0;
    uint64_t total_fees = 0;

    uint64_t total_rewards = 0;

    // Store accumulated distribution results
    std::map<name, uint64_t> validator_rewards;
    std::map<name, uint64_t> synchronizer_rewards;

    for (auto height = start_height; height <= end_height; height++) {
        // check feestat
        auto _resc_feestat_itr = _resc_feestat.find(height);
        check(_resc_feestat_itr != _resc_feestat.end(),
              "gasfund.xsat::distribute: feestat not found for height " + std::to_string(height));

        // calculate share of fees
        auto fees = _resc_feestat_itr->total_fee.amount;

        // Prevent overflow in fee calculations
        auto enf_fees = safe_pct(fees, config.enf_reward_rate, RATE_BASE_10000);
        auto rams_fees = safe_pct(fees, config.rams_reward_rate, RATE_BASE_10000);

        // Ensure consensus_fees doesn't underflow
        uint64_t fee_sum = safemath::add(enf_fees, rams_fees);
        uint64_t consensus_fees = (fee_sum <= fees) ? (fees - fee_sum) : 0;
        print("height: ", height, " fees: ", fees, " enf_fees: ", enf_fees, " rams_fees: ", rams_fees, " consensus_fees: ", consensus_fees, "---");

        total_fees = safemath::add(total_fees, fees);
        total_enf_fees = safemath::add(total_enf_fees, enf_fees);
        total_rams_fees = safemath::add(total_rams_fees, rams_fees);
        total_consensus_fees = safemath::add(total_consensus_fees, consensus_fees);

        // Calculate fees distribution but don't update database yet
        auto reward_result = calculate_reward(height);
        total_rewards += reward_result.total_rewards;

        // Accumulate fees for each receiver
        for (const auto& distribution : reward_result.distributions) {
            if (distribution.is_synchronizer) {
                synchronizer_rewards[distribution.receiver] += distribution.reward;
            } else {
                validator_rewards[distribution.receiver] += distribution.reward;
            }
        }
    }

    auto evm_start_height = _feestat.last_evm_height;
    auto _distribute_itr = _distributes.find(evm_start_height);
    if (_distribute_itr == _distributes.end()) {
        // Create distribution record
        _distributes.emplace(get_self(), [&](auto& row) {
            row.start_height = evm_start_height;
            row.total_fees = asset(total_fees, BTC_SYMBOL);
            row.enf_fees = asset(total_enf_fees, BTC_SYMBOL);
            row.rams_fees = asset(total_rams_fees, BTC_SYMBOL);
            row.consensus_fees = asset(total_consensus_fees, BTC_SYMBOL);
            row.total_xsat_rewards = asset(total_rewards, XSAT_SYMBOL);
        });
    } else {
        _distributes.modify(_distribute_itr, get_self(), [&](auto& row) {
            row.total_fees += asset(total_fees, BTC_SYMBOL);
            row.enf_fees += asset(total_enf_fees, BTC_SYMBOL);
            row.rams_fees += asset(total_rams_fees, BTC_SYMBOL);
            row.consensus_fees += asset(total_consensus_fees, BTC_SYMBOL);
            row.total_xsat_rewards += asset(total_rewards, XSAT_SYMBOL);
        });
    }

    // Check for valid rewards before processing
    if (total_rewards == 0) {
        // Log event but don't process further
        print("No rewards to distribute");
        return;
    }

    distribute_detail_table _distribute_details(get_self(), evm_start_height);
    auto _distribute_details_index = _distribute_details.get_index<"byreceiver"_n>();

    // Process validator rewards
    for (const auto& [validator, reward] : validator_rewards) {
        if (reward == 0)
            continue; // Skip zero rewards

        // 创建临时变量复制当前循环的值
        name current_validator = validator;
        uint64_t current_reward = reward;

        auto existing_record = _distribute_details_index.find(current_validator.value);
        if (existing_record == _distribute_details_index.end()) {
            _distribute_details.emplace(get_self(), [&](auto& row) {
                row.id = _distribute_details.available_primary_key();
                row.receiver = current_validator;
                row.receiver_type = "validator"_n;
                row.reward = asset(current_reward, XSAT_SYMBOL);
            });
        } else {
            // 先找到主索引中的迭代器
            auto primary_itr = _distribute_details.find(existing_record->id);
            _distribute_details.modify(
                primary_itr, get_self(), [&, current_reward](auto& row) { row.reward.amount += current_reward; });
        }
    }

    // Process synchronizer rewards
    for (const auto& [synchronizer, reward] : synchronizer_rewards) {
        if (reward == 0)
            continue; // Skip zero rewards

        // 创建临时变量复制当前循环的值
        name current_synchronizer = synchronizer;
        uint64_t current_reward = reward;

        auto existing_record = _distribute_details_index.find(current_synchronizer.value | XSAT_SCOPE_MASK);

        if (existing_record == _distribute_details_index.end()) {
            // synchronizer has no validator record, create new record
            _distribute_details.emplace(get_self(), [&](auto& row) {
                row.id = _distribute_details.available_primary_key();
                row.receiver = current_synchronizer;
                row.receiver_type = "synchronizer"_n;
                row.reward = asset(current_reward, XSAT_SYMBOL);
            });
        } else {
            // 先找到主索引中的迭代器
            auto primary_itr = _distribute_details.find(existing_record->id);
            // synchronizer has validator record, update existing record
            _distribute_details.modify(
                primary_itr, get_self(), [&, current_reward](auto& row) { row.reward.amount += current_reward; });
        }
    }

    // Process consensus fees for validators and synchronizers
    uint64_t _distribute_consensus_fees = 0;
    auto _consensus_fees_index = _consensus_fees.get_index<"byreceiver"_n>();

    // Process synchronizers first
    for (const auto& [receiver, reward] : synchronizer_rewards) {
        if (reward == 0 || total_rewards == 0)
            continue;

        // 创建临时变量复制当前循环的值
        name current_receiver = receiver;
        uint64_t current_reward = reward;

        uint64_t consensus_fees = safe_pct(total_consensus_fees, current_reward, total_rewards);
        if (consensus_fees == 0)
            continue;

        _distribute_consensus_fees = safemath::add(_distribute_consensus_fees, consensus_fees);

        auto _consensus_fees_itr = _consensus_fees_index.find(current_receiver.value | XSAT_SCOPE_MASK);
        if (_consensus_fees_itr == _consensus_fees_index.end()) {
            _consensus_fees.emplace(get_self(), [&, current_receiver, consensus_fees](auto& row) {
                row.id = _consensus_fees.available_primary_key();
                row.receiver = current_receiver;
                row.receiver_type = "synchronizer"_n;
                row.unclaimed = asset(consensus_fees, BTC_SYMBOL);
                row.total_claimed = asset(0, BTC_SYMBOL);
                row.last_claim_time = time_point_sec(); // Empty time point
            });
        } else {
            // 先找到主索引中的迭代器
            auto primary_itr = _consensus_fees.find(_consensus_fees_itr->id);
            _consensus_fees.modify(primary_itr, get_self(), [&, consensus_fees](auto& row) {
                row.unclaimed.amount = safemath::add(row.unclaimed.amount, consensus_fees);
            });
        }
    }

    // Now process validators
    auto remaining_consensus_fees = total_consensus_fees - _distribute_consensus_fees;
    if (validator_rewards.size() > 0 && remaining_consensus_fees > 0) {
        auto last_validator = validator_rewards.rbegin()->first;

        for (auto it = validator_rewards.begin(); it != validator_rewards.end(); ++it) {
            const auto& receiver = it->first;
            const auto& reward = it->second;

            if (reward == 0 || total_rewards == 0)
                continue;

            name current_receiver = receiver;
            uint64_t current_reward = reward;

            uint64_t consensus_fees;

            // For the last validator, assign remaining fees to avoid rounding errors
            if (current_receiver == last_validator) {
                consensus_fees = remaining_consensus_fees;
            } else {
                consensus_fees = safe_pct(total_consensus_fees, current_reward, total_rewards);
                remaining_consensus_fees -= consensus_fees;
            }

            if (consensus_fees == 0)
                continue;

            auto _consensus_fees_itr = _consensus_fees_index.find(current_receiver.value);
            if (_consensus_fees_itr == _consensus_fees_index.end()) {
                _consensus_fees.emplace(get_self(), [&, current_receiver, consensus_fees](auto& row) {
                    row.id = _consensus_fees.available_primary_key();
                    row.receiver = current_receiver;
                    row.receiver_type = "validator"_n;
                    row.unclaimed = asset(consensus_fees, BTC_SYMBOL);
                    row.total_claimed = asset(0, BTC_SYMBOL);
                    row.last_claim_time = time_point_sec(); // Empty time point
                });
            } else {
                // 先找到主索引中的迭代器
                auto primary_itr = _consensus_fees.find(_consensus_fees_itr->id);
                _consensus_fees.modify(
                    primary_itr, get_self(), [&, consensus_fees](auto& row) { row.unclaimed.amount += consensus_fees; });
            }
        }
    }

    // Update fees statistics
    _feestat.last_height = end_height;
    _feestat.enf_unclaimed = asset(safemath::add(_feestat.enf_unclaimed.amount, total_enf_fees), BTC_SYMBOL);
    _feestat.rams_unclaimed = asset(safemath::add(_feestat.rams_unclaimed.amount, total_rams_fees), BTC_SYMBOL);
    _feestat.consensus_unclaimed = asset(safemath::add(_feestat.consensus_unclaimed.amount, total_consensus_fees), BTC_SYMBOL);
    _feestat.total_consensus_fees = asset(safemath::add(_feestat.total_consensus_fees.amount, total_consensus_fees), BTC_SYMBOL);
    _feestat.total_enf_fees = asset(safemath::add(_feestat.total_enf_fees.amount, total_enf_fees), BTC_SYMBOL);
    _feestat.total_rams_fees = asset(safemath::add(_feestat.total_rams_fees.amount, total_rams_fees), BTC_SYMBOL);
    _fees_stat.set(_feestat, get_self());

    // Log successful distribution
    distributlog({.start_height = start_height,
                  .end_height = end_height,
                  .total_fees = asset(total_fees, BTC_SYMBOL),
                  .enf_fees = asset(total_enf_fees, BTC_SYMBOL),
                  .rams_fees = asset(total_rams_fees, BTC_SYMBOL),
                  .consensus_fees = asset(total_consensus_fees, BTC_SYMBOL),
                  .total_xsat_rewards = asset(total_rewards, XSAT_SYMBOL),
                  .distribute_time = current_time_point()});
}

gasfund::reward_calculation_result gasfund::calculate_reward(uint64_t height) {
    reward_distribution::reward_log_table _reward_log(REWARD_DISTRIBUTION_CONTRACT, REWARD_DISTRIBUTION_CONTRACT.value);
    reward_distribution::reward_log_table _xsat_reward_log(REWARD_DISTRIBUTION_CONTRACT, "xsat"_n.value);

    // Check BTC reward log
    auto _reward_log_itr = _reward_log.find(height);
    check(_reward_log_itr != _reward_log.end(),
          "gasfund.xsat::calculate_reward: BTC reward log not found for height " + std::to_string(height));

    // Check XSAT reward log
    auto _xsat_reward_log_itr = _xsat_reward_log.find(height);
    check(_xsat_reward_log_itr != _xsat_reward_log.end(),
          "gasfund.xsat::calculate_reward: XSAT reward log not found for height " + std::to_string(height));

    // Calculate rewards from both logs
    uint64_t sync_reward = _reward_log_itr->synchronizer_rewards.amount;
    uint64_t staking_reward = _reward_log_itr->staking_rewards.amount;
    uint64_t consensus_reward = _xsat_reward_log_itr->consensus_rewards.amount;

    // Prepare result structure
    reward_calculation_result result;
    result.total_rewards = sync_reward + staking_reward + consensus_reward;

    // Add synchronizer rewards if any
    if (sync_reward > 0) {
        result.distributions.push_back(
            {.receiver = _reward_log_itr->synchronizer, .reward = sync_reward, .is_synchronizer = true});
    }

    // Calculate BTC validator rewards if any
    if (staking_reward > 0) {
        auto btc_distributions = calculate_btc_validator_rewards(_reward_log_itr, staking_reward);
        result.distributions.insert(result.distributions.end(), btc_distributions.begin(), btc_distributions.end());
    }

    // Calculate XSAT validator rewards if any
    if (consensus_reward > 0) {
        auto xsat_distributions = calculate_xsat_validator_rewards(_xsat_reward_log_itr, consensus_reward);
        result.distributions.insert(result.distributions.end(), xsat_distributions.begin(), xsat_distributions.end());
    }

    return result;
}

vector<gasfund::reward_distribution_result>
gasfund::calculate_btc_validator_rewards(const reward_distribution::reward_log_table::const_iterator& _reward_log_itr,
                                         uint64_t staking_reward) {

    vector<reward_distribution_result> result;

    // Handle BTC validator rewards (based on staking ratios)
    const auto& validators = _reward_log_itr->provider_validators;
    const auto total_validators = validators.size();

    // Early return if no validators or rewards
    if (total_validators == 0 || staking_reward == 0) {
        return result;
    }

    // Check for valid endorsed staking
    check(_reward_log_itr->endorsed_staking > 0, "gasfund.xsat::calculate_btc_validator_rewards: Endorsed staking is zero");

    uint64_t total_assigned_reward = 0;

    // Process all validators
    for (size_t i = 0; i < total_validators; i++) {
        const auto& validator = validators[i];
        uint64_t validator_reward = 0;

        // For the last validator, assign remaining rewards to avoid rounding errors
        if (i == total_validators - 1) {
            validator_reward = staking_reward - total_assigned_reward;
        } else {
            // Calculate rewards based on staking ratio safely
            validator_reward = safe_pct(staking_reward, validator.staking, _reward_log_itr->endorsed_staking);
            total_assigned_reward = safemath::add(total_assigned_reward, validator_reward);
        }

        // Only add to result if rewards are positive
        if (validator_reward > 0) {
            result.push_back({.receiver = validator.account, .reward = validator_reward, .is_synchronizer = false});
        }
    }

    return result;
}

vector<gasfund::reward_distribution_result>
gasfund::calculate_xsat_validator_rewards(const reward_distribution::reward_log_table::const_iterator& _xsat_reward_log_itr,
                                          uint64_t consensus_reward) {

    vector<reward_distribution_result> result;

    // Handle XSAT consensus validators (evenly distributed)
    if (consensus_reward == 0) {
        return result;
    }

    const auto& xsat_validators = _xsat_reward_log_itr->provider_validators;
    const auto xsat_total_validators = xsat_validators.size();

    if (xsat_total_validators == 0) {
        return result;
    }

    // Calculate base amount per validator (evenly distributed)
    uint64_t total_xsat_assigned_reward = 0;
    uint64_t base_reward_per_validator = safemath::div(consensus_reward, xsat_total_validators);

    // Process all XSAT validators
    for (size_t i = 0; i < xsat_total_validators; i++) {
        const auto& validator = xsat_validators[i];
        uint64_t validator_reward = 0;

        // For the last validator, assign remaining rewards to avoid rounding errors
        if (i == xsat_total_validators - 1) {
            check(consensus_reward >= total_xsat_assigned_reward,
                  "gasfund.xsat::calculate_xsat_validator_rewards: Insufficient remaining rewards");
            validator_reward = consensus_reward - total_xsat_assigned_reward;
        } else {
            validator_reward = base_reward_per_validator;
            total_xsat_assigned_reward = safemath::add(total_xsat_assigned_reward, validator_reward);
        }

        // Only add to result if rewards are positive
        if (validator_reward > 0) {
            result.push_back({.receiver = validator.account, .reward = validator_reward, .is_synchronizer = false});
        }
    }

    return result;
}

[[eosio::on_notify("*::transfer")]]
void gasfund::on_transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    require_auth(get_self());
    if (to != get_self())
        return;

    if (from == get_self())
        return;

    if (get_first_receiver() == BTC_CONTRACT && quantity.symbol == BTC_SYMBOL) {
        // Handle BTC transfer
        handle_evm_fees_transfer(from, to, quantity, memo);
    }
}

void gasfund::handle_evm_fees_transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    // Handle BTC transfer
    if (memo != GASFUND_MEMO) {
        return;
    }
    auto config = _config.get_or_default();
    auto _fee_stat = _fees_stat.get_or_default();

    // chain state
    utxo_manage::chain_state_table _chain_state(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto chain_state = _chain_state.get();
    check(chain_state.irreversible_height > _fee_stat.last_evm_height,
          "gasfund.xsat::handle_evm_fees_transfer: current height is less than last evm height");

    auto start_height = _fee_stat.last_evm_height;
    auto end_height = chain_state.irreversible_height;

    // read distribute
    auto _distribute_itr = _distributes.find(start_height);
    check(_distribute_itr != _distributes.end(), "gasfund.xsat::handle_evm_fees_transfer: distribute record not found for height "
                                                     + std::to_string(_fee_stat.last_evm_height));

    // read distribute details
    distribute_detail_table _distribute_details(get_self(), start_height);
    auto _distribute_details_itr = _distribute_details.begin();
    check(
        _distribute_details_itr != _distribute_details.end(),
        "gasfund.xsat::handle_evm_fees_transfer: distribute details record not found for height " + std::to_string(start_height));

    auto total_rewards = _distribute_itr->total_xsat_rewards;
    auto total_fees = quantity;
    auto remaining_fees = total_fees;

    // 找到最后一个接收者，需要遍历获取
    name last_receiver;
    for (auto itr = _distribute_details.begin(); itr != _distribute_details.end(); ++itr) {
        last_receiver = itr->receiver;
    }

    for (auto itr = _distribute_details.begin(); itr != _distribute_details.end(); itr++) {
        auto _fees = safe_pct(total_fees.amount, itr->reward.amount, total_rewards.amount);
        if (itr->receiver == last_receiver) {
            _fees = remaining_fees.amount;
        }
        remaining_fees = asset(safemath::sub(remaining_fees.amount, _fees), remaining_fees.symbol);

        auto _consensus_fees_itr = _consensus_fees.find(itr->get_receiver_id());
        if (_consensus_fees_itr == _consensus_fees.end()) {
            _consensus_fees.emplace(get_self(), [&](auto& row) {
                row.id = _consensus_fees.available_primary_key();
                row.receiver = itr->receiver;
                row.receiver_type = itr->receiver_type;
                row.unclaimed = asset(_fees, BTC_SYMBOL);
                row.total_claimed = asset(0, BTC_SYMBOL);
            });
        } else {
            _consensus_fees.modify(
                _consensus_fees_itr, get_self(), [&](auto& row) { row.unclaimed += asset(_fees, BTC_SYMBOL); });
        }
    }
}

[[eosio::action]]
void gasfund::evmenfclaim(const name& caller, const checksum160& proxy_address, const checksum160& sender) {
    require_auth(caller);

    auto config = _config.get_or_default();
    check(config.evm_proxy_address != checksum160(), "gasfund.xsat::evmenfclaim: proxy address is not set");
    check(proxy_address == config.evm_proxy_address, "gasfund.xsat::evmenfclaim: invalid proxy address");

    check(config.enf_reward_address != checksum160(), "gasfund.xsat::evmenfclaim: enf reward address is not set");

    auto _feestat = _fees_stat.get_or_default();
    check(_feestat.enf_unclaimed.amount > 0, "gasfund.xsat::evmenfclaim: enf unclaimed is zero");

    // send fees

    auto unclaimed = _feestat.enf_unclaimed;
    _feestat.enf_unclaimed = asset(0, BTC_SYMBOL);
    _fees_stat.set(_feestat, get_self());
    // 发送日志
    evmenfclog(caller, proxy_address, sender, unclaimed);
}

[[eosio::action]]
void gasfund::evmramsclaim(const name& caller, const checksum160& proxy_address, const checksum160& sender) {
    require_auth(caller);

    auto config = _config.get_or_default();
    check(config.evm_proxy_address != checksum160(), "gasfund.xsat::evmramsclaim: proxy address is not set");
    check(proxy_address == config.evm_proxy_address, "gasfund.xsat::evmramsclaim: invalid proxy address");

    check(config.rams_reward_address != checksum160(), "gasfund.xsat::evmramsclaim: rams reward address is not set");
    auto _feestat = _fees_stat.get_or_default();
    check(_feestat.rams_unclaimed.amount > 0, "gasfund.xsat::evmramsclaim: rams unclaimed is zero");

    auto unclaimed = _feestat.rams_unclaimed;
    // TODO: send fees

    _feestat.rams_unclaimed = asset(0, BTC_SYMBOL);
    _fees_stat.set(_feestat, get_self());

    evmramsclog(caller, proxy_address, sender, unclaimed);
}


void gasfund::token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo) {
    btc::transfer_action transfer(value.contract, {from, "active"_n});
    transfer.send(from, to, value.quantity, memo);
}