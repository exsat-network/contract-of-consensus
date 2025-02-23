#include <blkendt.xsat/blkendt.xsat.hpp>
#include <utxomng.xsat/utxomng.xsat.hpp>
#include <endrmng.xsat/endrmng.xsat.hpp>
#include <rescmng.xsat/rescmng.xsat.hpp>
#include "../internal/safemath.hpp"

#ifdef DEBUG
#include "./src/debug.hpp"
#endif

//@auth utxomng.xsat
[[eosio::action]]
void block_endorse::erase(const uint64_t height) {
    require_auth(UTXO_MANAGE_CONTRACT);

    block_endorse::endorsement_table _endorsement(get_self(), height);
    auto endorsement_itr = _endorsement.begin();
    while (endorsement_itr != _endorsement.end()) {
        endorsement_itr = _endorsement.erase(endorsement_itr);
    }
}

//@auth get_self()
[[eosio::action]]
void block_endorse::config(const uint64_t limit_endorse_height, const uint16_t limit_num_endorsed_blocks,
                           const uint16_t min_validators, const uint64_t xsat_stake_activation_height,
                           const uint16_t consensus_interval_seconds, const asset& min_xsat_qualification) {
    require_auth(get_self());
    check(min_validators > 0, "blkendt.xsat::config: min_validators must be greater than 0");
    check(min_xsat_qualification.symbol == XSAT_SYMBOL,
          "blkendt.xsat::config: min_xsat_qualification symbol must be XSAT");
    check(min_xsat_qualification.amount > 0 && min_xsat_qualification.amount < XSAT_SUPPLY,
          "blkendt.xsat::config: min_xsat_qualification must be greater than 0BTC and less than 21000000XSAT");

    auto config = _config.get_or_default();
    config.limit_endorse_height = limit_endorse_height;
    config.xsat_stake_activation_height = xsat_stake_activation_height;
    config.limit_num_endorsed_blocks = limit_num_endorsed_blocks;
    config.min_validators = min_validators;
    config.consensus_interval_seconds = consensus_interval_seconds;
    config.min_xsat_qualification = min_xsat_qualification;
    _config.set(config, get_self());
}

//@auth validator
[[eosio::action]]
void block_endorse::endorse(const name& validator, const uint64_t height, const checksum256& hash) {
    check(has_auth(validator) || has_auth(get_self()), "1000:blkendt.xsat::endorse: validator or contract auth required");

    // Verify whether the endorsement height exceeds limit_endorse_height, 0 means no limit
    auto config = _config.get();
    check(config.limit_endorse_height == 0 || config.limit_endorse_height >= height,
          "1001:blkendt.xsat::endorse: the current endorsement status is disabled");

    utxo_manage::chain_state_table _chain_state(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);

    // Blocks that are already irreversible do not need to be endorsed
    auto chain_state = _chain_state.get();
    check(chain_state.irreversible_height < height && chain_state.migrating_height != height,
          "1002:blkendt.xsat::endorse: the current block is irreversible and does not need to be endorsed");

    // The endorsement height cannot exceed the interval of the parsed block height.
    check(
        config.limit_num_endorsed_blocks == 0 || chain_state.parsed_height + config.limit_num_endorsed_blocks >= height,
        "1003:blkendt.xsat::endorse: the endorsement height cannot exceed height "
            + std::to_string(chain_state.parsed_height + config.limit_num_endorsed_blocks));

    // fee deduction
    resource_management::pay_action pay(RESOURCE_MANAGE_CONTRACT, {get_self(), "active"_n});
    pay.send(height, hash, validator, ENDORSE, 1);

    block_endorse::endorsement_table _endorsement(get_self(), height);
    auto endorsement_idx = _endorsement.get_index<"byhash"_n>();
    auto endorsement_itr = endorsement_idx.find(hash);
    bool reached_consensus = false;
    if (endorsement_itr == endorsement_idx.end()) {
        // Verify whether the endorsement time of the next height is reached
        if (config.consensus_interval_seconds > 0 && height - 1 > START_HEIGHT) {
            utxo_manage::consensus_block_table _consensus_block(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
            auto consensus_block_idx = _consensus_block.get_index<"byheight"_n>();
            auto consensus_itr = consensus_block_idx.find(height - 1);
            if (consensus_itr == consensus_block_idx.end()) {
                check(false, "1008:blkendt.xsat::endorse: the next endorsement time has not yet been reached");
            } else {
                auto next_endorse_time = consensus_itr->created_at + config.consensus_interval_seconds;
                check(next_endorse_time <= current_time_point(),
                      "1008:blkendt.xsat::endorse: the next endorsement time has not yet been reached "
                          + next_endorse_time.to_string());
            }
        }

        bool xsat_stake_active
            = config.xsat_stake_activation_height > 0 && height >= config.xsat_stake_activation_height;
        // Obtain qualified validators based on the pledge amount.
        // If the block height of the activated xsat pledge amount is reached, directly switch to xsat pledge, otherwise use the btc pledge amount.
        std::vector<requested_validator_info> requested_validators
            = xsat_stake_active ? get_valid_validator_by_xsat_stake(config.min_xsat_qualification.amount)
                                : get_valid_validator_by_btc_stake();
        check(requested_validators.size() >= config.min_validators,
              "1004:blkendt.xsat::endorse: the number of valid validators must be greater than or equal to "
                  + std::to_string(config.min_validators));

        auto itr = std::find_if(requested_validators.begin(), requested_validators.end(),
                                [&](const requested_validator_info& a) {
                                    return a.account == validator;
                                });
        auto err_msg = xsat_stake_active ? "1005:blkendt.xsat::endorse: the validator has less than "
                                               + config.min_xsat_qualification.to_string() + " staked"
                                         : "1005:blkendt.xsat::endorse: the validator has less than 100 BTC staked";
        check(itr != requested_validators.end(), err_msg);
        provider_validator_info provider_info{
            .account = itr->account, .staking = itr->staking, .created_at = current_time_point()};
        requested_validators.erase(itr);
        auto endt_itr = _endorsement.emplace(get_self(), [&](auto& row) {
            row.id = _endorsement.available_primary_key();
            row.hash = hash;
            row.provider_validators.push_back(provider_info);
            row.requested_validators = requested_validators;
        });
        reached_consensus = endt_itr->num_reached_consensus() <= endt_itr->provider_validators.size();
    } else {
        check(std::find_if(endorsement_itr->provider_validators.begin(), endorsement_itr->provider_validators.end(),
                           [&](const provider_validator_info& a) {
                               return a.account == validator;
                           })
                  == endorsement_itr->provider_validators.end(),
              "1006:blkendt.xsat::endorse: validator is on the list of provider validators");

        auto itr = std::find_if(endorsement_itr->requested_validators.begin(),
                                endorsement_itr->requested_validators.end(), [&](const requested_validator_info& a) {
                                    return a.account == validator;
                                });
        check(itr != endorsement_itr->requested_validators.end(),
              "1007:blkendt.xsat::endorse: the validator has less than 100 BTC staked");
        endorsement_idx.modify(endorsement_itr, same_payer, [&](auto& row) {
            row.provider_validators.push_back(
                {.account = itr->account, .staking = itr->staking, .created_at = current_time_point()});
            row.requested_validators.erase(itr);
        });
        reached_consensus = endorsement_itr->num_reached_consensus() <= endorsement_itr->provider_validators.size();
    }

    if (reached_consensus) {
        utxo_manage::consensus_action _consensus(UTXO_MANAGE_CONTRACT, {get_self(), "active"_n});
        _consensus.send(height, hash);
    }
}

std::vector<block_endorse::requested_validator_info> block_endorse::get_valid_validator_by_btc_stake() {
    endorse_manage::validator_table _validator
        = endorse_manage::validator_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto idx = _validator.get_index<"byqualifictn"_n>();
    auto itr = idx.lower_bound(MIN_BTC_STAKE_FOR_VALIDATOR);
    std::vector<requested_validator_info> result;
    while (itr != idx.end()) {
        result.emplace_back(
            requested_validator_info{.account = itr->owner, .staking = static_cast<uint64_t>(itr->quantity.amount)});
        itr++;
    }
    return result;
}

std::vector<block_endorse::requested_validator_info> block_endorse::get_valid_validator_by_xsat_stake(
    const uint64_t min_xsat_qualification) {
    endorse_manage::validator_table _validator
        = endorse_manage::validator_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto idx = _validator.get_index<"bystakedxsat"_n>();
    auto itr = idx.lower_bound(min_xsat_qualification);
    std::vector<requested_validator_info> result;
    while (itr != idx.end()) {
        if (itr->quantity.amount > 0) {
            result.emplace_back(requested_validator_info{.account = itr->owner,
                                                         .staking = static_cast<uint64_t>(itr->quantity.amount)});
        }
        itr++;
    }
    return result;
}