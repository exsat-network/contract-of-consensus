#include <custody.xsat/custody.xsat.hpp>
#include <endrmng.xsat/endrmng.xsat.hpp>
#include <utxomng.xsat/utxomng.xsat.hpp>

#ifdef DEBUG
#include <bitcoin/script/address.hpp>

#include "./src/debug.hpp"
#endif

[[eosio::action]]
void custody::addcustody(const checksum160 staker, const checksum160 proxy, const name validator, const optional<string> btc_address,
                         optional<vector<uint8_t>> scriptpubkey) {
    require_auth(get_self());
    check(is_account(validator), "custody.xsat::addcustody: validator does not exists");
    check(proxy != checksum160(), "custody.xsat::addcustody: proxy cannot be empty");
    check(xsat::utils::is_proxy_valid(proxy), "custody.xsat::addcustody: proxy is not valid");
    check(btc_address.has_value() || scriptpubkey.has_value(), "custody.xsat::addcustody: btc_address and scriptpubkey cannot be empty at the same time");

    endorse_manage::credit_proxy_table _proxy(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto proxy_hash = xsat::utils::compute_id(proxy);
    auto proxy_idx = _proxy.get_index<"byproxy"_n>();
    proxy_idx.require_find(proxy_hash, "custody.xsat::addcustody: proxy does not exists");

    auto staker_idx = _custody.get_index<"bystaker"_n>();
    auto staker_itr = staker_idx.find(xsat::utils::compute_id(staker));
    check(staker_itr == staker_idx.end(), "custody.xsat::addcustody: staker address already exists");

    if (btc_address.has_value() && !btc_address->empty()) {
        scriptpubkey = bitcoin::utility::address_to_scriptpubkey(*btc_address);
    }
    check(scriptpubkey.has_value() && !scriptpubkey->empty(), "custody.xsat::addcustody: scriptpubkey cannot be empty");

    auto scriptpubkey_idx = _custody.get_index<"scriptpubkey"_n>();
    const checksum256 hash = xsat::utils::hash(*scriptpubkey);
    auto scriptpubkey_itr = scriptpubkey_idx.find(hash);
    check(scriptpubkey_itr == scriptpubkey_idx.end(), "custody.xsat::addcustody: bitcoin address already exists");

    _custody.emplace(get_self(), [&](auto& row) {
        row.id = next_custody_id();
        row.staker = staker;
        row.proxy = proxy;
        row.validator = validator;
        row.value = 0;
        row.latest_stake_time = eosio::current_time_point();
        if (btc_address.has_value()) {
            row.btc_address = *btc_address;
            row.scriptpubkey = *scriptpubkey;
        } else {
            row.scriptpubkey = *scriptpubkey;
        }
    });
}

[[eosio::action]]
void custody::delcustody(const checksum160 staker) {
    require_auth(get_self());
    auto staker_id = xsat::utils::compute_id(staker);
    auto staker_idx = _custody.get_index<"bystaker"_n>();
    auto staker_itr = staker_idx.require_find(staker_id, "custody.xsat::delcustody: staker does not exists");
    uint64_t current_staking_value = get_current_staking_value(staker_itr);
    if (current_staking_value > 0) {
        endorse_manage::creditstake_action creditstake(ENDORSER_MANAGE_CONTRACT, {get_self(), "active"_n});
        creditstake.send(staker_itr->proxy, staker_itr->staker, staker_itr->validator, asset(0, BTC_SYMBOL));
    }
    staker_idx.erase(staker_itr);
}

[[eosio::action]]
void custody::creditstake(const checksum160& staker, const uint64_t balance) {
    require_auth(get_self());
    auto staker_id = xsat::utils::compute_id(staker);
    auto staker_idx = _custody.get_index<"bystaker"_n>();
    auto staker_itr = staker_idx.require_find(staker_id, "custody.xsat::creditstake: staker does not exists");
    handle_staking(staker_itr, balance);
}

void custody::config(const uint64_t valid_blocks) {
    require_auth(get_self());
    check(valid_blocks > 0, "custody.xsat::config: valid_blocks must be greater than 0");

    global_row global = _global.get_or_default();
    global.valid_blocks = valid_blocks;
    _global.set(global, get_self());
}

uint64_t custody::enroll(const name& account) {
    require_auth(account);
    endorse_manage::validator_table _validator(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto validator_itr = _validator.require_find(account.value, "custody.xsat::enroll: account is not a validator account");
    auto active_flag = validator_itr->active_flag.has_value() ? validator_itr->active_flag.value() : 0;
    auto role = validator_itr->role.has_value() ? validator_itr->role.value() : 0;
    check(active_flag != 1, "custody.xsat::enroll: account is already actived");
    check(role == 0, "custody.xsat::enroll: account is not a btc validator account");

    utxo_manage::chain_state_table _chain_state(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto chain_state = _chain_state.get();

    global_row global = _global.get_or_default();
    uint64_t valid_blocks = global.valid_blocks.has_value() ? global.valid_blocks.value() : 1008;

    const checksum256 transaction_id = xsat::utils::get_trx_id();
    uint64_t random = 0;
    memcpy(&random, transaction_id.extract_as_byte_array().data(), sizeof(uint64_t));
    random = 10000 + (random % 90001);

    auto itr = _enrollment.find(account.value);
    if (itr != _enrollment.end()) {
        check(!itr->is_valid, "custody.xsat::enroll: account already enrolled");
        if (chain_state.head_height < itr->end_height) {
            check(false, "custody.xsat::enroll: enrollment is not expired yet, no need to enroll again");
        } else {
            _enrollment.modify(itr, same_payer, [&](auto& row) {
                row.btc_address.clear();
                row.txid = checksum256();
                row.start_height = chain_state.head_height;
                row.end_height = chain_state.head_height + valid_blocks;
                row.random = random;
                row.is_valid = false;
            });
        }
    } else {
        _enrollment.emplace(get_self(), [&](auto& row) {
            row.account = account;
            row.random = random;
            row.start_height = chain_state.head_height;
            row.end_height = chain_state.head_height + valid_blocks;
            row.is_valid = false;
        });
    }
    return random;
}

void custody::verifytx(const name& account, const string& btc_address, const checksum256& txid, const string& information) {
    require_auth(account);
    check(xsat::utils::is_bitcoin_address(btc_address), "custody.xsat::verifytx: invalid bitcoin address");
    auto enroll_itr = _enrollment.require_find(account.value, "custody.xsat::verifytx: account not enrolled");
    check(!enroll_itr->is_valid, "custody.xsat::verifytx: account already verified");
    utxo_manage::chain_state_table _chain_state(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto chain_state = _chain_state.get();
    check(chain_state.head_height <= enroll_itr->end_height, "custody.xsat::verifytx: the txid verification has expired; please enroll again");

    utxo_manage::utxo_table _utxo = utxo_manage::utxo_table(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    utxo_manage::pending_utxo_table _pending_utxo = utxo_manage::pending_utxo_table(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto utxo_idx = _utxo.get_index<"byutxoid"_n>();
    bool is_valid = false;
    uint32_t real_index = 0;
    for (uint32_t index = 0; index <= 50; index++) {
        checksum256 utxo_id = xsat::utils::compute_utxo_id(txid, index);
        auto utxo_itr = utxo_idx.find(utxo_id);
        if (utxo_itr != utxo_idx.end() && utxo_itr->value % 100000 == enroll_itr->random) {
            is_valid = true;
            real_index = index;
            break;
        }
    }
    if (!is_valid) {
        auto pending_utxo_idx = _pending_utxo.get_index<"byutxoid"_n>();
        for (uint32_t index = 0; index <= 50; index++) {
            checksum256 utxo_id = xsat::utils::compute_utxo_id(txid, index);
            auto pending_utxo_itr = pending_utxo_idx.find(utxo_id);
            if (pending_utxo_itr != pending_utxo_idx.end() && pending_utxo_itr->value % 100000 == enroll_itr->random) {
                is_valid = true;
                break;
            }
        }
    }
    check(is_valid, "custody.xsat::verifytx: the txid is not valid");
    _enrollment.modify(enroll_itr, same_payer, [&](auto& row) {
        row.btc_address = btc_address;
        row.txid = txid;
        row.index = real_index;
        row.is_valid = true;
        row.information = information;
    });
}

template <typename T>
uint64_t custody::get_current_staking_value(T& itr) {
    endorse_manage::evm_staker_table _staking(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    checksum256 staking_id = endorse_manage::compute_staking_id(itr->proxy, itr->staker, itr->validator);
    auto staking_idx = _staking.get_index<"bystakingid"_n>();
    auto staking_itr = staking_idx.find(staking_id);
    return staking_itr != staking_idx.end() ? staking_itr->quantity.amount : 0;
}

template <typename T>
void custody::handle_staking(T& itr, uint64_t balance) {
    uint64_t current_staking_value = get_current_staking_value(itr);
    uint64_t new_staking_value = balance >= MAX_STAKING ? MAX_STAKING : 0;
    check(new_staking_value != current_staking_value, "custody.xsat::handle_staking: no change in staking value");

    // credit stake
    endorse_manage::creditstake_action creditstake(ENDORSER_MANAGE_CONTRACT, {get_self(), "active"_n});
    creditstake.send(itr->proxy, itr->staker, itr->validator, asset(new_staking_value, BTC_SYMBOL));
    auto staker_itr = _custody.require_find(itr->id, "custody.xsat::handle_staking: staker does not exists");
    _custody.modify(staker_itr, same_payer, [&](auto& row) {
        row.value = new_staking_value;
        row.latest_stake_time = eosio::current_time_point();
    });
}

uint64_t custody::next_custody_id() {
    global_row global = _global.get_or_default();
    global.custody_id++;
    _global.set(global, get_self());
    return global.custody_id;
}
