template <typename T>
void reward_distribution::clear_table(T& table, uint64_t rows_to_clear) {
    auto itr = table.begin();
    while (itr != table.end() && rows_to_clear--) {
        itr = table.erase(itr);
    }
}

[[eosio::action]]
void reward_distribution::cleartable(const name table_name, const optional<name> scope,
                                     const optional<uint64_t> max_rows) {
    require_auth(get_self());
    const uint64_t rows_to_clear = (!max_rows || *max_rows == 0) ? -1 : *max_rows;
    const uint64_t value = scope ? scope->value : get_self().value;

    reward_balance_table _reward_balance(get_self(), value);

    // tables
    if (table_name == "rewardlogs"_n)
        clear_table(_reward_log, rows_to_clear);
    else if (table_name == "rewardbal"_n)
        _reward_balance.remove();
    else
        check(false, "rwddist.xsat::cleartable: [table_name] unknown table to clear");
}
