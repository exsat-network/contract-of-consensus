#include <brdgmng.xsat/brdgmng.xsat.hpp>

#ifdef DEBUG
// #include <bitcoin/script/address.hpp>
#include "./src/debug.hpp"
#endif

[[eosio::action]]
void brdgmng::initialize() {
    require_auth(BOOT_ACCOUNT);
    boot_row boot = _boot.get_or_default();
    check(!boot.initialized, "brdgmng.xsat::boot: already initialized");
    // initialize issue 1 BTC
    btc::issue_action issue(BTC_CONTRACT, {BTC_CONTRACT, "active"_n});
    asset quantity = asset(BTC_BASE, BTC_SYMBOL);
    issue.send(BTC_CONTRACT, quantity, "initialize issue one BTC to boot.xsat");
    // transfer BTC to boot.xsat
    btc::transfer_action transfer(BTC_CONTRACT, {BTC_CONTRACT, "active"_n});
    transfer.send(BTC_CONTRACT, BOOT_ACCOUNT, quantity, "transfer one BTC to boot.xsat");
    boot.initialized = true;
    boot.returned = false;
    boot.quantity = quantity;
    _boot.set(boot, get_self());
}

[[eosio::action]]
void brdgmng::updateconfig(bool deposit_enable, bool withdraw_enable, bool check_uxto_enable, uint64_t limit_amount, uint64_t deposit_fee,
                           uint64_t withdraw_fast_fee, uint64_t withdraw_slow_fee, uint16_t withdraw_merge_count, uint16_t withdraw_timeout_minutes,
                           uint16_t btc_address_inactive_clear_days) {
    require_auth(get_self());
    config_row config = _config.get_or_default();
    config.deposit_enable = deposit_enable;
    config.withdraw_enable = withdraw_enable;
    config.check_uxto_enable = check_uxto_enable;
    config.limit_amount = limit_amount;
    config.deposit_fee = deposit_fee;
    config.withdraw_fast_fee = withdraw_fast_fee;
    config.withdraw_slow_fee = withdraw_slow_fee;
    config.withdraw_merge_count = withdraw_merge_count;
    config.withdraw_timeout_minutes = withdraw_timeout_minutes;
    config.btc_address_inactive_clear_days = btc_address_inactive_clear_days;
    _config.set(config, get_self());
}

[[eosio::action]]
void brdgmng::addperm(const uint64_t id, const vector<name>& actors) {
    require_auth(get_self());
    if (actors.empty()) {
        check(false, "brdgmng.xsat::addperm: actors cannot be empty");
    }
    auto itr = _permission.find(id);
    check(itr == _permission.end(), "brdgmng.xsat::addperm: permission id already exists");
    _permission.emplace(get_self(), [&](auto& row) {
        row.id = id;
        row.actors = actors;
    });
}

[[eosio::action]]
void brdgmng::delperm(const uint64_t id) {
    require_auth(get_self());
    auto itr = _permission.require_find(id, "brdgmng.xsat::delperm: permission id does not exists");
    _permission.erase(itr);
}

[[eosio::action]]
void brdgmng::addaddresses(const name& actor, const uint64_t permission_id, string b_id, string wallet_code, const vector<string>& btc_addresses) {
    require_auth(actor);
    if (btc_addresses.empty()) {
        check(false, "brdgmng.xsat::addaddresses: btc_addresses cannot be empty");
    }
    check_permission(actor, permission_id);

    auto address_idx = _address.get_index<"bybtcaddr"_n>();
    auto address_mapping_idx = _address_mapping.get_index<"bybtcaddr"_n>();
    for (const auto& btc_address : btc_addresses) {
        checksum256 btc_addrress_hash = xsat::utils::hash(btc_address);
        auto address_itr = address_idx.find(btc_addrress_hash);
        check(address_itr == address_idx.end(), "brdgmng.xsat::addaddresses: btc_address already exists in address");
        auto address_mapping_itr = address_mapping_idx.find(btc_addrress_hash);
        check(address_mapping_itr == address_mapping_idx.end(), "brdgmng.xsat::addaddresses: btc_address already exists in address_mapping");

        _address.emplace(get_self(), [&](auto& row) {
            row.id = next_address_id();
            row.permission_id = permission_id;
            row.b_id = b_id;
            row.wallet_code = wallet_code;
            row.provider_actors = vector<name>{actor};
            row.statuses = vector<address_status>{address_status_initiated};
            row.confirmed_count = 1;
            row.status = global_status_initiated;
            row.btc_address = btc_address;
        });
    }
    statistics_row statistics = _statistics.get_or_default();
    statistics.total_btc_address_count += btc_addresses.size();
    _statistics.set(statistics, get_self());
}

[[eosio::action]]
void brdgmng::valaddress(const name& actor, const uint64_t permission_id, const uint64_t address_id, const address_status status) {
    check_permission(actor, permission_id);

    auto address_itr = _address.require_find(address_id, "brdgmng.xsat::valaddress: address_id does not exists in address");
    check(address_itr->status != global_status_succeed, "brdgmng.xsat::valaddress: btc_address status is already confirmed");

    auto actor_itr = std::find_if(address_itr->provider_actors.begin(), address_itr->provider_actors.end(), [&](const auto& u) { return u == actor; });
    check(actor_itr == address_itr->provider_actors.end(), "brdgmng.xsat::valaddress: actor already exists in the provider actors list");
    _address.modify(address_itr, same_payer, [&](auto& row) {
        row.provider_actors.push_back(actor);
        row.statuses.push_back(status);
        if (status == address_status_confirmed) {
            row.confirmed_count++;
        }
        if (verifyProviders(_permission.get(permission_id).actors, address_itr->provider_actors)) {
            if (row.confirmed_count == address_itr->provider_actors.size()) {
                row.status = global_status_succeed;
            } else {
                row.status = global_status_failed;
            }
        }
    });
}

[[eosio::action]]
void brdgmng::mappingaddr(const name& actor, const uint64_t permission_id, const checksum160 evm_address) {
    check_permission(actor, permission_id);

    auto evm_address_mapping_idx = _address_mapping.get_index<"byevmaddr"_n>();
    checksum256 evm_address_hash = xsat::utils::compute_id(evm_address);
    auto evm_address_mapping_itr = evm_address_mapping_idx.find(evm_address_hash);
    check(evm_address_mapping_itr == evm_address_mapping_idx.end(), "brdgmng.xsat::mappingaddr: evm_address already exists in address_mapping");

    // get the btc_address from the address according to the permission_id, which needs to be a confirmed address
    auto address_idx = _address.get_index<"bypermission"_n>();
    auto address_lower = address_idx.lower_bound(permission_id);
    auto address_upper = address_idx.upper_bound(permission_id);
    check(address_lower != address_upper, "brdgmng.xsat::mappingaddr: permission id does not exists in address");
    auto address_itr = address_lower;
    string btc_address;
    for (; address_itr != address_upper; ++address_itr) {
        if (address_itr->status == global_status_succeed) {
            btc_address = address_itr->btc_address;
            break;
        }
    }
    check(!btc_address.empty(), "brdgmng.xsat::mappingaddr: the confirmed btc address with permission id does not exists in address");

    auto btc_address_mapping_idx = _address_mapping.get_index<"bybtcaddr"_n>();
    checksum256 btc_address_hash = xsat::utils::hash(btc_address);
    auto btc_address_mapping_itr = btc_address_mapping_idx.find(btc_address_hash);
    check(btc_address_mapping_itr == btc_address_mapping_idx.end(), "brdgmng.xsat::mappingaddr: btc_address already exists in address_mapping");

    _address_mapping.emplace(get_self(), [&](auto& row) {
        row.id = next_address_mapping_id();
        row.b_id = address_itr->b_id;
        row.wallet_code = address_itr->wallet_code;
        row.btc_address = btc_address;
        row.evm_address = evm_address;
    });

    auto address_id_itr = _address.require_find(address_itr->id, "brdgmng.xsat::mappingaddr: address id does not exists in address");
    _address.erase(address_id_itr);

    statistics_row statistics = _statistics.get_or_default();
    statistics.mapped_address_count++;
    _statistics.set(statistics, get_self());
}

[[eosio::action]]
void brdgmng::deposit(const name& actor, const uint64_t permission_id, const string& b_id, const string& wallet_code, const string& btc_address,
                      const string& order_id, const string& order_no, const uint64_t block_height, const string& tx_id, const uint64_t amount,
                      const optional<string>& remark_detail, const uint64_t tx_time_stamp, const uint64_t create_time_stamp) {
    check_permission(actor, permission_id);
    check(_config.get_or_default().deposit_enable, "brdgmng.xsat::deposit: deposit is disabled");

    // check order_id is unique in the deposit table
    deposit_index _deposit_pending = deposit_index(_self, pending_scope);
    auto deposit_idx_pending = _deposit_pending.get_index<"byorderid"_n>();
    auto deposit_itr_pending = deposit_idx_pending.find(xsat::utils::hash(order_id));
    check(deposit_itr_pending == deposit_idx_pending.end(), "brdgmng.xsat::deposit: order_id already exists in deposit");

    deposit_index _deposit_confirmed = deposit_index(_self, confirmed_scope);
    auto deposit_idx_confirmed = _deposit_confirmed.get_index<"byorderid"_n>();
    auto deposit_itr_confirmed = deposit_idx_confirmed.find(xsat::utils::hash(order_id));
    check(deposit_itr_confirmed == deposit_idx_confirmed.end(), "brdgmng.xsat::deposit: order_id already exists in deposit");

    _deposit_pending.emplace(get_self(), [&](auto& row) {
        row.id = next_deposit_id();
        row.permission_id = permission_id;
        row.provider_actors = vector<provider_actor_info>{provider_actor_info{actor, tx_status_initiated}};
        row.global_status = global_status_initiated;
        row.b_id = b_id;
        row.wallet_code = wallet_code;
        row.btc_address = btc_address;
        row.order_id = order_id;
        row.order_no = order_no;
        row.block_height = block_height;
        row.tx_id = tx_id;
        row.amount = amount;
        if (remark_detail.has_value()) {
            row.remark_detail = *remark_detail;
        }
        row.tx_time_stamp = tx_time_stamp;
        row.create_time_stamp = create_time_stamp;
    });
}

[[eosio::action]]
void brdgmng::valdeposit(const name& actor, const uint64_t permission_id, const uint64_t deposit_id, const tx_status tx_status,
                         const optional<string>& remark_detail) {
    check_permission(actor, permission_id);
    deposit_index _deposit_pending = deposit_index(_self, pending_scope);
    deposit_index _deposit_confirmed = deposit_index(_self, confirmed_scope);
    auto deposit_itr_confirmed = _deposit_confirmed.find(deposit_id);
    if (deposit_itr_confirmed != _deposit_confirmed.end()) {
        check(deposit_itr_confirmed->global_status != global_status_succeed, "brdgmng.xsat::valdeposit: deposit status is already succeed");
        check(deposit_itr_confirmed->global_status != global_status_failed, "brdgmng.xsat::valdeposit: deposit status is already failed");
    }
    auto deposit_itr_pending = _deposit_pending.require_find(deposit_id, "brdgmng.xsat::valdeposit: deposit id does not exists");

    auto actor_itr =
        std::find_if(deposit_itr_pending->provider_actors.begin(), deposit_itr_pending->provider_actors.end(), [&](const auto& u) { return u.actor == actor; });

    _deposit_pending.modify(deposit_itr_pending, same_payer, [&](auto& row) {
        if (actor_itr != deposit_itr->provider_actors.end()) {
            // actor exist, check the corresponding tx_status
            check(tx_status != tx_status_confirmed && tx_status != tx_status_failed && tx_status != tx_status_rollbacked,
                  "brdgmng.xsat::valdeposit: the tx_status is final and cannot be modified");
            actor_itr->tx_status = tx_status;
        } else {
            // actor not exist, add the actor and tx_status
            row.provider_actors.push_back({actor, tx_status});
        }
        if (tx_status == tx_status_confirmed) {
            row.confirmed_count++;
        } else if (tx_status == tx_status_failed || tx_status == tx_status_rollbacked) {
            row.global_status = global_status_failed;
        }
        if (remark_detail.has_value()) {
            row.remark_detail = *remark_detail;
        }
        if (verifyProviders(_permission.get(permission_id).actors, row.provider_actors)) {
            if (row.confirmed_count == row.provider_actors.size()) {
                row.global_status = global_status_succeed;
            }
        }
    });
    //move pending deposit to confirmed deposit
    if (deposit_itr_pending->global_status == global_status_succeed || deposit_itr_pending->global_status == global_status_failed) {
        if (deposit_itr_pending->global_status == global_status_succeed) {
            handle_btc_deposit(deposit_itr_pending->amount, deposit_itr_pending->evm_address);
        }
        _deposit_confirmed.emplace(get_self(), [&](auto& row) {
            row.id = deposit_itr_pending->id;
            row.permission_id = deposit_itr_pending->permission_id;
            row.provider_actors = deposit_itr_pending->provider_actors;
            row.global_status = deposit_itr_pending->status;
            row.b_id = deposit_itr_pending->b_id;
            row.wallet_code = deposit_itr_pending->wallet_code;
            row.btc_address = deposit_itr_pending->btc_address;
            row.order_id = deposit_itr_pending->order_id;
            row.order_no = deposit_itr_pending->order_no;
            row.block_height = deposit_itr_pending->block_height;
            row.tx_id = deposit_itr_pending->tx_id;
            row.amount = deposit_itr_pending->amount;
            row.remark_detail = deposit_itr_pending->remark_detail;
            row.tx_time_stamp = deposit_itr_pending->tx_time_stamp;
            row.create_time_stamp = deposit_itr_pending->create_time_stamp;
        });
        _deposit_pending.erase(deposit_itr_pending);
    }
}

[[eosio::on_notify("*::transfer")]]
void brdgmng::on_transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    // ignore transfers
    if (to != get_self()) return;

    const name contract = get_first_receiver();
    check(contract == BTC_CONTRACT && quantity.symbol == BTC_SYMBOL, "brdgmng.xsat: only transfer [btc.xsat/BTC]");
    if (from == BOOT_ACCOUNT) {
        do_return(from, contract, quantity, memo);
    } else {
        do_withdraw(from, contract, quantity, memo);
    }
}

void brdgmng::do_return(const name& from, const name& contract, const asset& quantity, const string& memo) {
    check(from == BOOT_ACCOUNT, "brdgmng.xsat: only return from [boot.xsat]");
    boot_row boot = _boot.get_or_default();
    check(!boot.returned, "brdgmng.xsat: already returned");
    check(quantity == boot.quantity, "brdgmng.xsat: refund amount does not match, must be " + boot.quantity.to_string());
    // burn BTC
    btc::transfer_action transfer(contract, {get_self(), "active"_n});
    transfer.send(get_self(), BTC_CONTRACT, quantity, "return BTC to btc.xsat");
    btc::retire_action retire(contract, {BTC_CONTRACT, "active"_n});
    retire.send(quantity, "retire BTC from boot.xsat");

    boot.returned = true;
    _boot.set(boot, get_self());
}

void brdgmng::do_withdraw(const name& from, const name& contract, const asset& quantity, const string& memo) {
    check(_config.get_or_default().withdraw_enable, "brdgmng.xsat: withdraw is disabled");
    check(quantity.amount > 0, "brdgmng.xsat: must transfer positive quantity");
    auto parts = xsat::utils::split(memo, ",");
    auto INVALID_MEMO = "brdgmng.xsat: invalid memo ex: \"<btc_address>,<gas_level>\"";
    check(parts.size() == 2, INVALID_MEMO);
    withdraw_index _withdraw_pending = withdraw_index(_self, pending_scope);
    uint64_t withdraw_id = next_withdraw_id();
    _withdraw_pending.emplace(get_self(), [&](auto& row) {
        row.id = withdraw_id;
        row.btc_address = parts[0];
        row.gas_level = parts[1];
        row.amount = quantity.amount;
        row.status = global_status_initiated;
        if (gas_level == "fast") {
            row.order_no = generate_order_no({withdraw_id});
        }
    });
}

[[eosio::action]]
void brdgmng::genorderno() {
    withdraw_index _withdraw_pending = withdraw_index(_self, pending_scope);
    auto withdraw_idx_pending = _withdraw_pending.get_index<"byorderno"_n>();
    //根据lower_bound和upper_bound查找order_no为空的记录
    checksum160 order_no_hash = xsat::utils::hash_ripemd160("");
    auto lower_bound = withdraw_idx_pending.lower_bound(order_no_hash);
    auto upper_bound = withdraw_idx_pending.upper_bound(order_no_hash);
    // for (auto itr = lower_bound; itr != upper_bound; ++itr) {
    //     _withdraw_pending.modify(itr, same_payer, [&](auto& row) {
    //         row.order_no = generate_order_no({itr->id});
    //     });
    // }

}

[[eosio::action]]
void brdgmng::withdrawinfo(const name& actor, const uint64_t permission_id, const uint64_t withdraw_id, const string& b_id, const string& wallet_code,
                     const string& order_id, const uint64_t block_height, const string& tx_id, const optional<string>& remark_detail,
                     const uint64_t tx_time_stamp, const uint64_t create_time_stamp) {
    check_permission(actor, permission_id);
    withdraw_index _withdraw_pending = withdraw_index(_self, pending_scope);
    auto withdraw_itr_pending = _withdraw_pending.require_find(withdraw_id, "brdgmng.xsat::withdrawinfo: withdraw id does not exists");
    check(withdraw_itr_pending->order_id.empty(), "brdgmng.xsat::withdrawinfo: order info already exists in withdraw");

    _withdraw_pending.modify(withdraw_itr_pending, same_payer, [&](auto& row) {
        row.b_id = b_id;
        row.wallet_code = wallet_code;
        row.order_id = order_id;
        row.block_height = block_height;
        row.tx_id = tx_id;
        row.tx_time_stamp = tx_time_stamp;
        row.create_time_stamp = create_time_stamp;
        if (remark_detail.has_value()) {
            row.remark_detail = *remark_detail;
        }
    });
}

// todo It has to be processed in batches according to the order number
[[eosio::action]]
void brdgmng::valwithdraw(const name& actor, const uint64_t permission_id, const uint64_t withdraw_id,
                     const withdraw_status withdraw_status, const uint8_t tx_status,
                     const optional<string>& remark_detail) {
    check_permission(actor, permission_id);
    withdraw_index _withdraw_pending = withdraw_index(_self, pending_scope);
    withdraw_index _withdraw_confirmed = withdraw_index(_self, confirmed_scope);
    auto withdraw_itr_confirmed = _withdraw_confirmed.find(withdraw_id);
    if (withdraw_itr_confirmed != _withdraw_confirmed.end()) {
        check(withdraw_itr_confirmed->global_status != global_status_succeed, "brdgmng.xsat::valwithdraw: withdraw status is already succeed");
        check(withdraw_itr_confirmed->global_status != global_status_failed, "brdgmng.xsat::valwithdraw: withdraw status is already failed");
    }
    auto withdraw_itr_pending = _withdraw_pending.require_find(withdraw_id, "brdgmng.xsat::valwithdraw: withdraw id does not exists");
    auto actor_itr = std::find_if(withdraw_itr_pending->provider_actors.begin(), withdraw_itr_pending->provider_actors.end(), [&](const auto& u) { return u.actor == actor; });
    check(withdraw_status >= withdraw_itr_pending->withdraw_status, "brdgmng.xsat::valwithdraw: the withdraw_status must increase forward");

    _withdraw_pending.modify(withdraw_itr_pending, same_payer, [&](auto& row) {
        row.withdraw_status = withdraw_status;
        if (actor_itr != withdraw_itr->provider_actors.end()) {
            // actor exist, check the corresponding tx_status
            check(tx_status != tx_status_confirmed && tx_status != tx_status_failed && tx_status != tx_status_rollbacked,
                  "brdgmng.xsat::valwithdraw: the tx_status is final and cannot be modified");
            actor_itr->tx_status = tx_status;
        } else {
            // actor not exist, add the actor and tx_status
            row.provider_actors.push_back({actor, tx_status});
        }
        if (tx_status == tx_status_confirmed) {
            row.confirmed_count++;
        } else if (tx_status == tx_status_failed || tx_status == tx_status_rollbacked) {
            row.status = global_status_failed;
        }
        if (remark_detail.has_value()) {
            row.remark_detail = *remark_detail;
        }
        if (verifyProviders(_permission.get(permission_id).actors, row.provider_actors)) {
            if (row.confirmed_count == row.provider_actors.size()) {
                row.status = global_status_succeed;
            }
        }
    });
    //move pending withdraw to confirmed withdraw
    if (withdraw_itr_pending->global_status == global_status_succeed || withdraw_itr_pending->global_status == global_status_failed) {
        if (withdraw_itr_pending->global_status == global_status_succeed) {
            handle_btc_withdraw(withdraw_itr_pending->amount);
        }
        _withdraw_confirmed.emplace(get_self(), [&](auto& row) {
            row.id = withdraw_itr_pending->id;
            row.permission_id = withdraw_itr_pending->permission_id;
            row.provider_actors = withdraw_itr_pending->provider_actors;
            row.confirmed_count = withdraw_itr_pending->confirmed_count;
            row.tx_status = withdraw_itr_pending->tx_status;
            row.withdraw_status = withdraw_itr_pending->withdraw_status;
            row.global_status = withdraw_itr_pending->global_status;
            row.b_id = withdraw_itr_pending->b_id;
            row.wallet_code = withdraw_itr_pending->wallet_code;
            row.btc_address = withdraw_itr_pending->btc_address;
            row.gas_level = withdraw_itr_pending->gas_level;
            row.order_id = withdraw_itr_pending->order_id;
            row.order_no = withdraw_itr_pending->order_no;
            row.block_height = withdraw_itr_pending->block_height;
            row.tx_id = withdraw_itr_pending->tx_id;
            row.amount = withdraw_itr_pending->amount;
            row.remark_detail = withdraw_itr_pending->remark_detail;
            row.tx_time_stamp = withdraw_itr_pending->tx_time_stamp;
            row.create_time_stamp = withdraw_itr_pending->create_time_stamp;
        });
        _withdraw_pending.erase(withdraw_itr_pending);
    }
}

void brdgmng::check_permission(const name& actor, const uint64_t permission_id) {
    require_auth(actor);
    auto permission_itr = _permission.require_find(permission_id, "brdgmng.xsat::check_permission: permission id does not exists");
    bool has_permission = false;
    for (const auto& item : permission_itr->actors) {
        if (actor == item) {
            has_permission = true;
            break;
        }
    }
    check(has_permission, "brdgmng.xsat::check_permission: actor does not have permission");
}

uint64_t brdgmng::next_address_id() {
    global_id_row global_id = _global_id.get_or_default();
    global_id.address_id++;
    _global_id.set(global_id, get_self());
    return global_id.address_id;
}

uint64_t brdgmng::next_address_mapping_id() {
    global_id_row global_id = _global_id.get_or_default();
    global_id.address_mapping_id++;
    _global_id.set(global_id, get_self());
    return global_id.address_mapping_id;
}

uint64_t brdgmng::next_deposit_id() {
    global_id_row global_id = _global_id.get_or_default();
    global_id.deposit_id++;
    _global_id.set(global_id, get_self());
    return global_id.deposit_id;
}

uint64_t brdgmng::next_withdraw_id() {
    global_id_row global_id = _global_id.get_or_default();
    global_id.withdraw_id++;
    _global_id.set(global_id, get_self());
    return global_id.withdraw_id;
}

bool brdgmng::verifyProviders(const std::vector<provider_actor_info>& requested_actors, const std::vector<provider_actor_info>& provider_actors) {
    for (const auto& request : requested_actors) {
        if (std::find(provider_actors.begin(), provider_actors.end(), request) == provider_actors.end()) {
            return false;
        }
    }
    return true;
}

void brdgmng::token_transfer(const name& from, const name& to, const extended_asset& value, const string& memo) {
    btc::transfer_action transfer(value.contract, {from, "active"_n});
    transfer.send(from, to, value.quantity, memo);
}

void brdgmng::handle_btc_deposit(const uint64_t amount, const checksum160& evm_address) {
    if (amount >= _config.get_or_default().limit_amount) {
        const uint64_t issue_amount = safemath::sub(amount, _config.get_or_default().deposit_fee);
        // issue BTC
        asset quantity = asset(issue_amount, BTC_SYMBOL);
        btc::issue_action issue(BTC_CONTRACT, {BTC_CONTRACT, "active"_n});
        issue.send(BTC_CONTRACT, quantity, "issue BTC to evm address");
        // transfer BTC to evm address
        btc::transfer_action transfer(BTC_CONTRACT, {BTC_CONTRACT, "active"_n});
        transfer.send(BTC_CONTRACT, ERC20_CONTRACT, quantity, "0x" + xsat::utils::sha1_to_hex(evm_address));

        statistics_row statistics = _statistics.get_or_default();
        statistics.total_deposit_amount += issue_amount;
        _statistics.set(statistics, get_self());
    }
}

void brdgmng::handle_btc_withdraw(const uint64_t amount) {
    if (amount >= _config.get_or_default().limit_amount) {
        // transfer BTC btc.xsat
        asset quantity = asset(amount, BTC_SYMBOL);
        btc::transfer_action transfer(BTC_CONTRACT, {get_self(), "active"_n});
        transfer.send(get_self(), BTC_CONTRACT, quantity, "transfer BTC to btc.xsat");
        // retire BTC
        btc::retire_action retire(BTC_CONTRACT, { BTC_CONTRACT, "active"_n });
        retire.send(quantity, "retire BTC from withdraw");

        statistics_row statistics = _statistics.get_or_default();
        statistics.total_withdraw_amount += amount;
        _statistics.set(statistics, get_self());
    }
}

checksum160 brdgmng::generate_order_no(const std::vector<uint64_t>& ids) {
    auto timestamp = current_time_point().sec_since_epoch();
    std::vector<uint64_t> sorted_ids = ids;
    std::sort(sorted_ids.begin(), sorted_ids.end());
    std::string order_no;
    for (size_t i = 0; i < sorted_ids.size(); ++i) {
        order_no += std::to_string(sorted_ids[i]);
        if (i < sorted_ids.size() - 1) {
            order_no += "-";
        }
    }
    order_no += "-" + std::to_string(timestamp);
    return xsat::utils::hash_ripemd160(order_no);
}