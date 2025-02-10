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

    // get endorsement scope
    bool xsat_validator = false;
    if (validator_itr->role.has_value() && validator_itr->role.value() == 1) {
        xsat_validator = true;
    }

    auto _endorse_scope = height;
    auto _other_endorse_scope = height | 0x100000000;

    if (xsat_validator) {

        _endorse_scope = height | 0x100000000;
        _other_endorse_scope = height;
    }

    block_endorse::endorsement_table _endorsement(get_self(), height);
    auto endorsement_idx = _endorsement.get_index<"byhash"_n>();
    auto endorsement_itr = endorsement_idx.find(hash);
    bool reached_consensus = false;

    // consensus config
    endorse_manage::consensus_config_table _consensus_config
        = endorse_manage::consensus_config_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto consensus_config = _consensus_config.get_or_default();

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
            = xsat_validator ? get_valid_validator_by_xsat_stake(consensus_config.xsat_base_stake.amount)
                                : get_valid_validator_by_btc_stake(consensus_config.btc_base_stake.amount);

        check(requested_validators.size() >= config.min_validators,
              "1004:blkendt.xsat::endorse: the number of valid validators must be greater than or equal to "
                  + std::to_string(config.min_validators));

        auto itr = std::find_if(requested_validators.begin(), requested_validators.end(),
                                [&](const requested_validator_info& a) {
                                    return a.account == validator;
                                });
        auto err_msg = xsat_validator ? "1005:blkendt.xsat::endorse: the validator has less than "
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

        // Need check other consensus
        if (consensus_config.version == 2) {

            // Get the number of other consensus and check
            block_endorse::endorsement_table _other_endorsement(get_self(), _endorse_scope);
            auto other_endorsement_idx = _other_endorsement.get_index<"byhash"_n>();
            auto other_endorsement_itr = other_endorsement_idx.find(hash);

            // xsat consensus & btc consensus need same and
            if (other_endorsement_itr == other_endorsement_idx.end() || other_endorsement_itr->num_reached_consensus() > endorsement_itr->provider_validators.size()) {

                return;
            }
        }

        utxo_manage::consensus_action _consensus(UTXO_MANAGE_CONTRACT, {get_self(), "active"_n});
        _consensus.send(height, hash);
    }
    
    // send endrmng.xsat::endorse
    endorse_manage::endorse_action _endorse(ENDORSER_MANAGE_CONTRACT, {get_self(), "active"_n});
    _endorse.send(validator, height);
}

std::vector<block_endorse::requested_validator_info> block_endorse::get_valid_validator_by_btc_stake(const uint64_t min_btc_qualification) {
    endorse_manage::validator_table _validator
        = endorse_manage::validator_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto idx = _validator.get_index<"byqualifictn"_n>();
    auto itr = idx.lower_bound(MIN_BTC_STAKE_FOR_VALIDATOR);
    std::vector<requested_validator_info> result;

    // endrmng.xsat consensus config
    endorse_manage::consensus_config_table _consensus_config
        = endorse_manage::consensus_config_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto consensus_config = _consensus_config.get_or_default();

    while (itr != idx.end()) {
        
        // only btc validator
        if (itr->role.has_value() && itr->role.value() != 0) {

            continue;
        }

        // check active
        if (itr->active_flag.has_value() && itr->active_flag.value() == 0) {

            continue;
        }

        if (itr->consecutive_vote_count.has_value() && itr->consecutive_vote_count.value() < consensus_config.validator_active_vote_count) {

            continue;
        }

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

    // endrmng.xsat consensus config
    endorse_manage::consensus_config_table _consensus_config
        = endorse_manage::consensus_config_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto consensus_config = _consensus_config.get_or_default();

    while (itr != idx.end()) {
        // only xsat validator
        if (itr->role.has_value() && itr->role.value() != 1) {

            continue;
        }

        // check active
        if (itr->active_flag.has_value() && itr->active_flag.value() == 0) {

            continue;
        }

        if (itr->consecutive_vote_count.has_value() && itr->consecutive_vote_count.value() < consensus_config.validator_active_vote_count) {

            continue;
        }

        result.emplace_back(requested_validator_info{.account = itr->owner, .staking = static_cast<uint64_t>(itr->quantity.amount)});
        itr++;
    }
    return result;
}

[[eosio::action]]
void block_endorse::revote(const name& synchronizer, const uint64_t height) {
    // 权限验证
    require_auth(synchronizer);

    // 验证同步节点
    pool::synchronizer_table _synchronizer(POOL_REGISTER_CONTRACT, POOL_REGISTER_CONTRACT.value);
    _synchronizer.require_find(synchronizer.value, "block_endorse::revote: not a synchronizer account");

    // 获取链状态，检查当前区块是否处于不可投票状态
    utxo_manage::chain_state_table _chain_state(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto chain_state = _chain_state.get();
    check(chain_state.irreversible_height < height && chain_state.migrating_height != height,
          "1002:blkendt.xsat::revote: the current block is irreversible");

    // 获取 revote_record 表实例（作用域为当前合约）
    revote_record_table _revote_record(get_self(), get_self().value);
    // 获取按 height 索引的子表
    auto height_index = _revote_record.get_index<"byheight"_n>();

    // 使用 lower_bound 与 upper_bound 获取所有高度为 height 的记录
    auto lb = height_index.lower_bound(height);
    auto ub = height_index.upper_bound(height);
    auto pending_itr = std::find_if(lb, ub, [](const auto& rec) {
        return rec.status == 0;
    });

    // 计算同步节点总数及达到共识所需票数
    const uint64_t total_synchronizers = std::distance(_synchronizer.begin(), _synchronizer.end());
    const uint64_t required_votes = (total_synchronizers / 2) + 1;

    // 记录当前时间（确保操作一致性）
    const auto now = time_point_sec(current_time_point());

    // 如果没有 pending 记录，则新建一条记录并记录首次投票
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

    // 检查当前同步节点是否已经投票，避免重复计票
    if (std::find(pending_itr->synchronizers.begin(),
                  pending_itr->synchronizers.end(),
                  synchronizer) != pending_itr->synchronizers.end()) {
        return;
    }

    // 更新记录，加入新的投票
    bool consensus_reached = false;
    height_index.modify(pending_itr, get_self(), [&](auto& rec) {
        rec.synchronizers.push_back(synchronizer);
        rec.updated_at = now;
        if (rec.synchronizers.size() >= required_votes) {
            rec.status = 1;
            consensus_reached = true;
        }
    });

    // 达成共识后，执行 endorsement 数据迁移（包括多个 scope 的处理）
    if (consensus_reached) {
        process_revote_consensus(height);
    }
}

void block_endorse::process_revote_consensus(const uint64_t height) {
    // BTC
    migrate_endorsements(height);
    // XSAT
    migrate_endorsements(height | 0x100000000);
}

void block_endorse::migrate_endorsements(const uint64_t src_scope) {
    endorsement_table src_endorsements(get_self(), src_scope);

    // 定义目标 scope，这里采用添加高位标记的方式（你也可以自定义其他算法）
    const uint64_t FINAL_MASK = 1ULL << 63;
    uint64_t dest_scope = src_scope | FINAL_MASK;
    endorsement_table dest_endorsements(get_self(), dest_scope);

    // 遍历源 endorsement 表，逐条复制到目标表后删除原记录
    auto it = src_endorsements.begin();
    while (it != src_endorsements.end()) {
        dest_endorsements.emplace(get_self(), [&](auto& new_row) {
            new_row = *it; // 整体拷贝 endorsement_row 结构体内容
        });
        it = src_endorsements.erase(it);
    }
}