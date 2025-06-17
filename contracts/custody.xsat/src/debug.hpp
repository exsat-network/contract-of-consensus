template <typename T>
void custody::clear_table(T& table, uint64_t rows_to_clear) {
    auto itr = table.begin();
    while (itr != table.end() && rows_to_clear--) {
        itr = table.erase(itr);
    }
}

[[eosio::action]]
void custody::cleartable(const name table_name, const optional<name> scope, const optional<uint64_t> max_rows) {
    require_auth(get_self());
    const uint64_t rows_to_clear = (!max_rows || *max_rows == 0) ? -1 : *max_rows;
    const uint64_t value = scope ? scope->value : get_self().value;

    if (table_name == "globals"_n) {
        _global.remove();
    } else if (table_name == "custodies"_n) {
        custody_index _custody(get_self(), get_self().value);
        clear_table(_custody, rows_to_clear);
    } else if (table_name == "enrollments"_n) {
        enrollment_index _enrollment(get_self(), get_self().value);
        clear_table(_enrollment, rows_to_clear);
    } else {
        check(false, "custody::cleartable: [table_name] unknown table to clear");
    }
}

[[eosio::action]]
void custody::addr2pubkey(const string& address) {
    auto scriptpubkey = bitcoin::utility::address_to_scriptpubkey(address);
    auto res = xsat::utils::bytes_to_hex(scriptpubkey);
    print_f("scriptpubkey result: %", res);
}

[[eosio::action]]
void custody::pubkey2addr(const vector<uint8_t> data) {
    std::vector<string> res;
    bool success = bitcoin::ExtractDestination(data, CHAIN_PARAMS, res);
    if (success) {
        print_f("address: %", res[0]);
    } else {
        print_f("failed to extract address");
    }
}

[[eosio::action]]
void custody::modifyrandom(const name& account, const uint64_t random) {
    require_auth(get_self());
    auto enroll_itr = _enrollment.require_find(account.value, "custody.xsat::modifyrandom: account not enrolled");
    _enrollment.modify(enroll_itr, same_payer, [&](auto& row) {
        row.random = random;
    });
    print_f("custody.xsat::modifyrandom: random value for % updated to %", account, random);
}