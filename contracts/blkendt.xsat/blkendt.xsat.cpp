#include <blkendt.xsat/blkendt.xsat.hpp>
#include <poolreg.xsat/poolreg.xsat.hpp>
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
                           const uint16_t min_validators, const uint16_t consensus_interval_seconds, 
                           const uint8_t validator_active_vote_count) {
    require_auth(get_self());
    check(min_validators > 0, "blkendt.xsat::config: min_validators must be greater than 0");

    auto config = _config.get_or_default();
    config.limit_endorse_height = limit_endorse_height;
    // config.xsat_stake_activation_height = xsat_stake_activation_height;
    config.limit_num_endorsed_blocks = limit_num_endorsed_blocks;
    config.min_validators = min_validators;
    config.consensus_interval_seconds = consensus_interval_seconds;
    // config.min_xsat_qualification = min_xsat_qualification;
    // config.min_btc_qualification = min_btc_qualification;
    // config.xsat_reward_height = xsat_reward_height;
    config.validator_active_vote_count = validator_active_vote_count;
    _config.set(config, get_self());
}
//@auth get_self()
[[eosio::action]] 
void block_endorse::setqualify(const asset& min_xsat_qualification, const asset& min_btc_qualification) {
    if (!has_auth(ENDORSER_MANAGE_CONTRACT)) {
        require_auth(get_self());
    }

    check(min_xsat_qualification.symbol == XSAT_SYMBOL,
          "blkendt.xsat::setqualify: min_xsat_qualification symbol must be XSAT");
    check(min_xsat_qualification.amount > 0 && min_xsat_qualification.amount < XSAT_SUPPLY,
          "blkendt.xsat::setqualify: min_xsat_qualification must be greater than 0BTC and less than 21000000XSAT");
    check(min_btc_qualification.symbol == BTC_SYMBOL,
          "blkendt.xsat::setqualify: min_btc_qualification symbol must be BTC");
    check(min_btc_qualification.amount > 0 && min_btc_qualification.amount < BTC_SUPPLY,
          "blkendt.xsat::setqualify: min_btc_qualification must be greater than 0BTC and less than 21000000BTC");

    auto config = _config.get_or_default();
    config.min_xsat_qualification = min_xsat_qualification;
    config.min_btc_qualification = min_btc_qualification;
    _config.set(config, get_self());
}

//@auth get_self()
[[eosio::action]]
void block_endorse::setconheight(const uint64_t xsat_stake_activation_height, const uint64_t xsat_reward_height) {
    require_auth(get_self());

    check(xsat_stake_activation_height > START_HEIGHT, "blkendt.xsat::setconheight: xsat_stake_activation_height must be greater than START_HEIGHT");
    check(xsat_reward_height > START_HEIGHT, "blkendt.xsat::setconheight: xsat_reward_height must be greater than START_HEIGHT");

    auto config = _config.get_or_default();
    config.xsat_stake_activation_height = xsat_stake_activation_height;
    config.xsat_reward_height = xsat_reward_height;
    _config.set(config, get_self());
}

//@auth validator
[[eosio::action]]
void block_endorse::endorse(const name& validator, const uint64_t height, const checksum256& hash) {
    require_auth(validator);

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

    // v2 check validator
    endorse_manage::validator_table _validator
        = endorse_manage::validator_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto validator_itr = _validator.require_find(validator.value, "blkendt.xsat::endorse: [validators] does not exists");

    auto validator_active_vote_count = config.get_validator_active_vote_count();
    
    // check revote record, if revote record is success, set validator_active_vote_count to 0
    revote_record_table _revote_record(get_self(), get_self().value);
    auto revote_record_idx = _revote_record.get_index<"byheight"_n>();
    auto revote_record_itr = revote_record_idx.find(height);
    auto is_revote = false;
    if (revote_record_itr != revote_record_idx.end() && revote_record_itr->status == 1) {
        validator_active_vote_count = 0;
        is_revote = true;
    }

    // check xsat consensus active
    if (config.is_xsat_consensus_active(height)) {

        check(validator_itr->active_flag.has_value() && validator_itr->active_flag.value() == 1, "1007:blkendt.xsat::endorse: validator is not active");
    }

    // check xsat reward active
    if (config.is_xsat_reward_active(height)) {

        // check consecutive vote count
        if (validator_itr->consecutive_vote_count.has_value() && validator_itr->consecutive_vote_count.value() < validator_active_vote_count) {

            // send endrmng.xsat::endorse       
            endorse_manage::endorse_action _endorse(ENDORSER_MANAGE_CONTRACT, {get_self(), "active"_n});
            _endorse.send(validator, height);
            return;
        }
    }

    // get endorsement scope
    bool xsat_validator = false;
    if (validator_itr->role.has_value() && validator_itr->role.value() == 1) {
        xsat_validator = true;
    }

    auto _endorse_scope = height;
    auto _other_endorse_scope = height | XSAT_SCOPE_MASK;

    if (xsat_validator) {

        _endorse_scope = height | XSAT_SCOPE_MASK;
        _other_endorse_scope = height;
    }

    block_endorse::endorsement_table _endorsement(get_self(), _endorse_scope);
    auto endorsement_idx = _endorsement.get_index<"byhash"_n>();
    auto endorsement_itr = endorsement_idx.find(hash);
    bool reached_consensus = false;

    auto err_msg = "1005:blkendt.xsat::endorse: validator not in requested_validators list";
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

        // Obtain qualified validators based on the pledge amount.
        // If the block height of the activated xsat pledge amount is reached, directly switch to xsat pledge, otherwise use the btc pledge amount.
        std::vector<requested_validator_info> requested_validators
            = xsat_validator ? get_valid_validator_by_xsat_stake(height, validator_active_vote_count)
                                : get_valid_validator_by_btc_stake(height, validator_active_vote_count);

        check(requested_validators.size() >= config.min_validators,
              "1004:blkendt.xsat::endorse: the number of valid validators must be greater than or equal to "
                  + std::to_string(config.min_validators));

        auto itr = std::find_if(requested_validators.begin(), requested_validators.end(),
                                [&](const requested_validator_info& a) {
                                    return a.account == validator;
                                });
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

        check(itr != endorsement_itr->requested_validators.end(), err_msg);
        endorsement_idx.modify(endorsement_itr, same_payer, [&](auto& row) {
            row.provider_validators.push_back(
                {.account = itr->account, .staking = itr->staking, .created_at = current_time_point()});
            row.requested_validators.erase(itr);
        });
        reached_consensus = endorsement_itr->num_reached_consensus() <= endorsement_itr->provider_validators.size();
    }
    
    // if not revote, send endorse action
    if (!is_revote) {
        // send endrmng.xsat::endorse
        endorse_manage::endorse_action _endorse(ENDORSER_MANAGE_CONTRACT, {get_self(), "active"_n});
        _endorse.send(validator, height);
    }

    if (reached_consensus) {
        // For consensus version 2 with XSAT consensus enabled, verify that the XSAT endorsement result
        // is consistent with the BTC endorsement result.
        if (config.is_xsat_consensus_active(height)) {
            // Retrieve the endorsement record from the XSAT consensus scope.
            block_endorse::endorsement_table other_endorsement(get_self(), _other_endorse_scope);
            auto other_endorsement_idx = other_endorsement.get_index<"byhash"_n>();
            auto other_endorsement_itr = other_endorsement_idx.find(hash);

            // If the XSAT endorsement record does not exist, or its number of reached consensus validators
            // exceeds the number of provider validators from the BTC endorsement, then do not proceed.
            if (other_endorsement_itr == other_endorsement_idx.end() ||
                other_endorsement_itr->num_reached_consensus() > other_endorsement_itr->provider_validators.size()) {
                
                return;
            }
        }

        // If consensus conditions are met, trigger the consensus action.
        utxo_manage::consensus_action consensus(UTXO_MANAGE_CONTRACT, {get_self(), "active"_n});
        consensus.send(height, hash);
    }
}

std::vector<block_endorse::requested_validator_info> block_endorse::get_valid_validator_by_btc_stake(const uint64_t height, const uint8_t consecutive_vote_count) {
    auto config = _config.get();
    auto min_btc_qualification = config.get_btc_base_stake().amount;

    endorse_manage::validator_table _validator
        = endorse_manage::validator_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto idx = _validator.get_index<"byqualifictn"_n>();
    auto itr = idx.lower_bound(min_btc_qualification);
    std::vector<requested_validator_info> result;

    while (itr != idx.end()) {
        
        // only btc validator
        if (itr->role.has_value() && itr->role.value() != 0) {

            itr++;
            continue;
        }

        // XSAT consensus actived
        if (config.is_xsat_consensus_active(height)) {
            
            // check active
            if (itr->active_flag.has_value() && itr->active_flag.value() == 0) {

                itr++;
                continue;
            }

            if (itr->consecutive_vote_count.value() < consecutive_vote_count) {

                itr++;
                continue;
            }
        }

        result.emplace_back(
            requested_validator_info{.account = itr->owner, .staking = static_cast<uint64_t>(itr->quantity.amount)});
        itr++;
    }
    return result;
}

std::vector<block_endorse::requested_validator_info> block_endorse::get_valid_validator_by_xsat_stake(const uint64_t height, const uint8_t consecutive_vote_count) {
    auto config = _config.get();

    endorse_manage::validator_table _validator
        = endorse_manage::validator_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto idx = _validator.get_index<"bystakedxsat"_n>();
    auto itr = idx.lower_bound(config.min_xsat_qualification.amount);
    std::vector<requested_validator_info> result;

    while (itr != idx.end()) {
        // only xsat validator
        if (!itr->role.has_value() || itr->role.value() != 1) {

            itr++;
            continue;
        }

        // XSAT consensus actived
        if (config.is_xsat_reward_active(height)) {

            // check active
            if (itr->active_flag.has_value() && itr->active_flag.value() == 0) {

                itr++;
                continue;
            }

            if (itr->consecutive_vote_count.value() < consecutive_vote_count) {

                itr++;
                continue;
            }
        }

        result.emplace_back(requested_validator_info{.account = itr->owner, .staking = static_cast<uint64_t>(itr->xsat_quantity.amount)});
        itr++;
    }
    return result;
}

[[eosio::action]]
void block_endorse::revote(const name& synchronizer, const uint64_t height) {
    require_auth(synchronizer);

    // check irreversible block height
    utxo_manage::chain_state_table _chain_state(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto chain_state = _chain_state.get();
    check(chain_state.irreversible_height < height && chain_state.migrating_height != height,
          "1002:blkendt.xsat::revote: the current block is irreversible");
          
    pool::synchronizer_table _synchronizer(POOL_REGISTER_CONTRACT, POOL_REGISTER_CONTRACT.value);
    auto synchronizer_itr = _synchronizer.require_find(synchronizer.value, "block_endorse::revote: not a synchronizer account");

    // check synchronizer latest producer height
    check(synchronizer_itr->produced_block_limit == 0
            || chain_state.head_height - synchronizer_itr->latest_produced_block_height <= synchronizer_itr->produced_block_limit,
        "2006:blksync.xsat::initbucket: to become a synchronizer, a block must be produced within 72 hours");

    revote_record_table _revote_record(get_self(), get_self().value);
    auto height_index = _revote_record.get_index<"byheight"_n>();

    // use lower_bound and upper_bound to find pending record
    auto lb = height_index.lower_bound(height);
    auto ub = height_index.upper_bound(height);
    auto pending_itr = std::find_if(lb, ub, [](const auto& rec) {
        return rec.status == 0;
    });

    // calculate required votes
    // filter active synchronizers
    std::vector<name> active_synchronizers;
    for(auto sync_itr = _synchronizer.begin(); sync_itr != _synchronizer.end(); sync_itr++) {
        if(sync_itr->produced_block_limit == 0 || 
           chain_state.head_height - sync_itr->latest_produced_block_height <= sync_itr->produced_block_limit) {
            active_synchronizers.push_back(sync_itr->synchronizer);
        }
    }

    const uint64_t total_synchronizers = active_synchronizers.size();
    const uint64_t required_votes = (total_synchronizers / 2) + 1;
    const auto now = time_point_sec(current_time_point());
    if (pending_itr == ub) {
        _revote_record.emplace(get_self(), [&](auto& rec) {
            rec.id            = _revote_record.available_primary_key();
            rec.height        = height;
            rec.synchronizers = { synchronizer };
            rec.status        = 0;
            rec.created_at    = now;
            rec.updated_at    = now;
        });
        return;
    }

    // check synchronizer in pending
    check(std::find(pending_itr->synchronizers.begin(),
                    pending_itr->synchronizers.end(),
                    synchronizer) == pending_itr->synchronizers.end(),
        "2007:blksync.xsat::revote: synchronizer already in pending record");

    // update pending record
    bool consensus_reached = false;
    height_index.modify(pending_itr, get_self(), [&](auto& rec) {
        rec.synchronizers.push_back(synchronizer);
        rec.updated_at = now;
        if (rec.synchronizers.size() >= required_votes) {
            rec.status = 1;
            consensus_reached = true;
        }
    });

    // check consensus reached and process
    if (consensus_reached) {
        process_revote_consensus(height);
    }
}

void block_endorse::process_revote_consensus(const uint64_t height) {
    // BTC
    migrate_endorsements(height);
    // XSAT
    migrate_endorsements(height | XSAT_SCOPE_MASK);
}

void block_endorse::migrate_endorsements(const uint64_t src_scope) {
    endorsement_table src_endorsements(get_self(), src_scope);

    const uint64_t FINAL_MASK = 1ULL << 63;
    uint64_t dest_scope = src_scope | FINAL_MASK;
    endorsement_table dest_endorsements(get_self(), dest_scope);

    print("migrate_endorsements: src_scope: ", src_scope, " dest_scope: ", dest_scope, "\n");
    // foreach in src_endorsements
    auto it = src_endorsements.begin();
    while (it != src_endorsements.end()) {
        dest_endorsements.emplace(get_self(), [&](auto& new_row) {
            new_row = *it; // copy endorsement_row content
            new_row.id = dest_endorsements.available_primary_key();
        });
        it = src_endorsements.erase(it);
    }
}