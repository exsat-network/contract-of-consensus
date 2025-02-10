#include <utxomng.xsat/utxomng.xsat.hpp>
#include <blksync.xsat/blksync.xsat.hpp>
#include <blkendt.xsat/blkendt.xsat.hpp>
#include <rwddist.xsat/rwddist.xsat.hpp>
#include <rescmng.xsat/rescmng.xsat.hpp>
#include <poolreg.xsat/poolreg.xsat.hpp>
#include <bitcoin/core/block_header.hpp>
#include <bitcoin/core/transaction.hpp>
#include <bitcoin/script/address.hpp>

#ifdef DEBUG
#include "./src/debug.hpp"
#endif

//@auth get_self()
[[eosio::action]]
void utxo_manage::init(const uint64_t height, const checksum256& hash, const checksum256& cumulative_work) {
    require_auth(get_self());

    check(!_chain_state.exists() || _chain_state.get().irreversible_height == 0,
          "utxomng.xsat::init: can only be initialized once");
    check(height == START_HEIGHT, "utxomng.xsat::init: block height must be 839999");
    check(hash > ZERO_HASH, "utxomng.xsat::init: invalid hash");
    check(cumulative_work > ZERO_HASH, "utxomng.xsat::init: invalid cumulative_work");

    auto chain_state = _chain_state.get_or_default();
    chain_state.head_height = height;
    chain_state.irreversible_height = height;
    chain_state.irreversible_hash = hash;
    chain_state.parsed_height = height;
    chain_state.status = waiting;
    _chain_state.set(chain_state, get_self());
}

//@auth get_self()
[[eosio::action]]
void utxo_manage::config(const uint16_t parse_timeout_seconds, const uint16_t num_validators_per_distribution,
                         const uint16_t retained_spent_utxo_blocks, const uint16_t num_retain_data_blocks,
                         const uint8_t num_merkle_layer, const uint16_t num_miner_priority_blocks) {
    require_auth(get_self());

    check(parse_timeout_seconds > 0, "utxomng.xsat::config: parse_timeout_seconds must be greater than 0");
    check(num_validators_per_distribution > 0,
          "utxomng.xsat::config: num_validators_per_distribution must be greater than 0");
    check(num_merkle_layer > 0, "utxomng.xsat::config: num_merkle_layer must be greater than 0");

    auto config = _config.get_or_default();
    config.parse_timeout_seconds = parse_timeout_seconds;
    config.num_validators_per_distribution = num_validators_per_distribution;
    config.num_retain_data_blocks = num_retain_data_blocks;
    config.retained_spent_utxo_blocks = retained_spent_utxo_blocks;
    config.num_merkle_layer = num_merkle_layer;
    config.num_txs_per_verification = 2 << (num_merkle_layer - 1);
    config.num_miner_priority_blocks = num_miner_priority_blocks;
    _config.set(config, get_self());
}

//@auth get_self()
[[eosio::action]]
void utxo_manage::addutxo(const uint64_t id, const checksum256& txid, const uint32_t index,
                          const vector<uint8_t>& scriptpubkey, const uint64_t value) {
    require_auth(get_self());

    auto chain_state = _chain_state.get_or_default();
    check(chain_state.head_height <= START_HEIGHT, "utxomng.xsat::addutxo: height must be less than or equal to 839999");

    auto utxo_idx = _utxo.get_index<"byutxoid"_n>();
    auto utxo_itr = utxo_idx.find(xsat::utils::compute_utxo_id(txid, index));
    if (utxo_itr == utxo_idx.end()) {
        _utxo.emplace(get_self(), [&](auto& row) {
            row.id = id;
            row.txid = txid;
            row.index = index;
            row.scriptpubkey = scriptpubkey;
            row.value = value;
        });
        chain_state.num_utxos += 1;
        _chain_state.set(chain_state, get_self());
    } else {
        utxo_idx.modify(utxo_itr, same_payer, [&](auto& row) {
            row.scriptpubkey = scriptpubkey;
            row.value = value;
        });
    }
}

//@auth get_self()
[[eosio::action]]
void utxo_manage::delutxo(const uint64_t id) {
    require_auth(get_self());

    auto chain_state = _chain_state.get_or_default();
    check(chain_state.head_height <= START_HEIGHT, "utxomng.xsat::delutxo: height must be less than or equal to 839999");

    auto& utxo = _utxo.get(id, "utxomng.xsat::delutxo: [utxos] does not exist");
    _utxo.erase(utxo);

    chain_state.num_utxos -= 1;
    _chain_state.set(chain_state, get_self());
}

//@auth get_self()
[[eosio::action]]
void utxo_manage::addblock(const uint64_t height, const checksum256& hash, const checksum256& cumulative_work,
                           const uint32_t version, const checksum256& previous_block_hash, const checksum256& merkle,
                           const uint32_t timestamp, const uint32_t bits, const uint32_t nonce) {
    require_auth(get_self());
    check(height <= START_HEIGHT, "utxomng.xsat::addblock: height must be less than or equal to 839999");

    auto block_itr = _block.find(height);
    if (block_itr == _block.end()) {
        _block.emplace(get_self(), [&](auto& row) {
            row.height = height;
            row.hash = hash;
            row.cumulative_work = cumulative_work;
            row.version = version;
            row.previous_block_hash = previous_block_hash;
            row.merkle = merkle;
            row.timestamp = timestamp;
            row.bits = bits;
            row.nonce = nonce;
        });
    } else {
        _block.modify(block_itr, same_payer, [&](auto& row) {
            row.height = height;
            row.hash = hash;
            row.cumulative_work = cumulative_work;
            row.version = version;
            row.previous_block_hash = previous_block_hash;
            row.merkle = merkle;
            row.timestamp = timestamp;
            row.bits = bits;
            row.nonce = nonce;
        });
    }
}

//@auth get_self()
[[eosio::action]]
void utxo_manage::delblock(const uint64_t height) {
    require_auth(get_self());
    check(height <= START_HEIGHT, "utxomng.xsat::delblock: height must be less than or equal to 839999");

    auto block_itr = _block.require_find(height, "utxomng.xsat::delblock: [blocks] does not exist");
    _block.erase(block_itr);
}

//@auth get_self()
[[eosio::action]]
void utxo_manage::delspentutxo(uint64_t rows, const uint64_t nonce) {
    require_auth(get_self());

    if (rows == 0)
        rows = -1;

    auto config = _config.get();
    auto chain_state = _chain_state.get();
    auto last_height = chain_state.irreversible_height - config.retained_spent_utxo_blocks;
    auto spent_utxo_idx = _spent_utxo.get_index<"byheight"_n>();
    auto spent_utxo_itr = spent_utxo_idx.lower_bound(START_HEIGHT);
    auto spent_utxo_end = spent_utxo_idx.upper_bound(last_height);

    while (spent_utxo_itr != spent_utxo_end && rows--) {
        spent_utxo_itr = spent_utxo_idx.erase(spent_utxo_itr);
    }
}

//@auth get_self()
[[eosio::action]]
void utxo_manage::delblockdata(uint64_t rows, const uint64_t nonce) {
    require_auth(get_self());

    if (rows == 0)
        rows = -1;

    block_sync::delchunks_action _delchunks(BLOCK_SYNC_CONTRACT, {get_self(), "active"_n});

    auto config = _config.get();
    auto chain_state = _chain_state.get();
    auto last_height = chain_state.irreversible_height - config.num_retain_data_blocks;
    auto block_extra_itr = _block_extra.lower_bound(START_HEIGHT);
    auto block_extra_end = _block_extra.upper_bound(last_height);

    while (block_extra_itr != block_extra_end && rows--) {
        _delchunks.send(block_extra_itr->bucket_id);
        block_extra_itr = _block_extra.erase(block_extra_itr);
    }
}

//@auth blksync.xsat or blkendt.xsat
[[eosio::action]]
void utxo_manage::consensus(const uint64_t height, const checksum256& hash) {
    if (!has_auth(BLOCK_SYNC_CONTRACT)) {
        require_auth(BLOCK_ENDORSE_CONTRACT);
    }

    block_sync::passed_index_table _passed_index(BLOCK_SYNC_CONTRACT, height);
    auto passed_index_idx = _passed_index.get_index<"byhash"_n>();
    auto passed_index_itr = passed_index_idx.find(hash);

    if (passed_index_itr == passed_index_idx.end()) {
        return;
    }

    // Check whether consensus has been reached based on endorsement data.
    if (!is_endorsement_consensus_reached(height, hash)) {
        return;
    }

    // Verify whether it exceeds the number of blocks miners preferentially produce.
    if (passed_index_itr->miner && passed_index_itr->synchronizer != passed_index_itr->miner) {
        block_sync::block_miner_table _block_miner(BLOCK_SYNC_CONTRACT, height);
        auto block_miner_idx = _block_miner.get_index<"byhash"_n>();
        auto block_miner_itr = block_miner_idx.require_find(hash);

        if (block_miner_itr->expired_block_num > current_block_number()) {
            return;
        }
    }

    if (passed_index_itr->miner) {
        block_sync::block_bucket_table _block_bucket(BLOCK_SYNC_CONTRACT, passed_index_itr->synchronizer.value);
        auto block_bucket_itr = _block_bucket.require_find(passed_index_itr->bucket_id,
                                                           "utxomng.xsat::consensus: block bucket does not exists");
        // update height and btc miners
        pool::updateheight_action _updateheight(POOL_REGISTER_CONTRACT, {get_self(), "active"_n});
        _updateheight.send(passed_index_itr->miner, height, block_bucket_itr->verify_info->btc_miners);
    }

    // get header
    auto block_data
        = block_sync::read_bucket(BLOCK_SYNC_CONTRACT, passed_index_itr->bucket_id, BLOCK_CHUNK, 0, BLOCK_HEADER_SIZE);
    eosio::datastream<const char*> block_stream(block_data.data(), block_data.size());
    bitcoin::core::block_header block_header;
    block_stream >> block_header;

    _consensus_block.emplace(get_self(), [&](auto& row) {
        row.height = height;
        row.hash = hash;
        row.version = block_header.version;
        row.previous_block_hash = bitcoin::be_checksum256_from_uint(block_header.previous_block_hash);
        row.merkle = bitcoin::be_checksum256_from_uint(block_header.merkle);
        row.timestamp = block_header.timestamp;
        row.bits = block_header.bits;
        row.nonce = block_header.nonce;
        row.cumulative_work = passed_index_itr->cumulative_work;
        row.bucket_id = passed_index_itr->bucket_id;
        row.miner = passed_index_itr->miner;
        row.synchronizer = passed_index_itr->synchronizer;
        row.created_at = current_time_point();
    });

    auto chain_state = _chain_state.get_or_default();
    // Set latest block height
    if (chain_state.head_height < height) {
        chain_state.head_height = height;
    }

    // Set the latest parsable block height
    auto config = _config.get();
    if (chain_state.parsing_height == height) {
        chain_state.parsing_height = height;
        chain_state.parsing_progress_of[hash]
            = {.bucket_id = passed_index_itr->bucket_id,
               .parser = passed_index_itr->synchronizer,
               .parse_expiration_time = current_time_point() + eosio::seconds(config.parse_timeout_seconds)};
    } else {
        find_set_next_parsable_block(chain_state, config.parse_timeout_seconds);
    }

    // Set the block height of the latest migration
    find_set_next_irreversible_block(chain_state);

    _chain_state.set(chain_state, get_self());

    // consensus
    block_sync::consensus_action block_sync_consensus(BLOCK_SYNC_CONTRACT, {get_self(), "active"_n});
    block_sync_consensus.send(height, passed_index_itr->synchronizer, passed_index_itr->bucket_id);
}

//---------------------------------------------------------------------
// Helper function: Check if endorsement consensus is reached
//---------------------------------------------------------------------
// This function reads the endorsement data from two scopes:
// 1. BTC endorsement table (scope = height)
// 2. XSAT endorsement table (scope = height | 0x100000000)
// It returns true if either endorsement table indicates consensus.
bool utxo_manage::is_endorsement_consensus_reached(const uint64_t height, const checksum256& hash) {
    // Check BTC endorsement (scope = height)
    block_endorse::endorsement_table endorsement_btc(BLOCK_ENDORSE_CONTRACT, height);
    auto endorsement_idx_btc = endorsement_btc.get_index<"byhash"_n>();
    auto itr_btc = endorsement_idx_btc.find(hash);
    bool btc_consensus = (itr_btc != endorsement_idx_btc.end() && itr_btc->reached_consensus());

    // check consensus version
    endorse_manage::consensus_config_table _consensus_config
        = endorse_manage::consensus_config_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto consensus_config = _consensus_config.get_or_default();

    // If the consensus configuration indicates an old consensus mechanism (version == 1)
    // or the xsat consensus flag is disabled (flags == 0),
    // then no new consensus process is initiated and we simply return the BTC consensus result.
    if (consensus_config.version == 1 || consensus_config.flags == 0) {
        return btc_consensus;
    }

    // Check XSAT endorsement (scope = height | 0x100000000)
    constexpr uint64_t XSAT_SCOPE_MASK = 0x100000000;
    block_endorse::endorsement_table endorsement_xsat(BLOCK_ENDORSE_CONTRACT, height | XSAT_SCOPE_MASK);
    auto endorsement_idx_xsat = endorsement_xsat.get_index<"byhash"_n>();
    auto itr_xsat = endorsement_idx_xsat.find(hash);
    bool xsat_consensus = (itr_xsat != endorsement_idx_xsat.end() && itr_xsat->reached_consensus());

    // Return true if both BTC and XSAT endorsements indicate consensus.
    return btc_consensus && xsat_consensus;
}

//@auth
[[eosio::action]]
utxo_manage::process_block_result utxo_manage::processblock(const name& synchronizer, uint64_t process_row,
                                                            const uint64_t nonce) {
    require_auth(synchronizer);

    auto chain_state = _chain_state.get();
    auto height = chain_state.parsing_height;
    check(height > 0, "4001:utxomng.xsat::processblock: there are currently no block to parse");

    // Find parsable hash
    auto current_time = current_time_point();
    checksum256 hash;
    for (const auto& it : chain_state.parsing_progress_of) {
        if (it.second.parser == synchronizer || it.second.parse_expiration_time <= current_time) {
            hash = it.first;
        }
    }

    check(hash != ZERO_HASH, "4003:utxomng.xsat::processblock: you are not a parser of the current block");

    // fee deduction
    resource_management::pay_action pay(RESOURCE_MANAGE_CONTRACT, {get_self(), "active"_n});
    pay.send(height, hash, synchronizer, PARSE, 1);

    auto config = _config.get();
    auto& parsing_progress = chain_state.parsing_progress_of[hash];

    // verify permissions and whether parsing times out
    if (parsing_progress.parse_expiration_time > current_time) {
        check(synchronizer == parsing_progress.parser,
              "4004:utxomng.xsat::processblock: you are not a parser of the current block");
    } else {
        pool::synchronizer_table _synchronizer(POOL_REGISTER_CONTRACT, POOL_REGISTER_CONTRACT.value);
        _synchronizer.require_find(synchronizer.value, "4005:utxomng.xsat::processblock: only synchronizers can parse");

        parsing_progress.parser = synchronizer;
        parsing_progress.parse_expiration_time = current_time + eosio::seconds(config.parse_timeout_seconds);
    }

    // Migrate irreversible block data first and then parse the latest block
    if (chain_state.status == waiting) {
        if (chain_state.migrating_height > 0) {
            chain_state.status = migrating;

            // issue reward
            reward_distribution::distribute_action _distribute(REWARD_DISTRIBUTION_CONTRACT, {get_self(), "active"_n});
            _distribute.send(chain_state.migrating_height);
        } else {
            chain_state.status = parsing;
        }
    }

    if (chain_state.status == migrating) {
        migrate(chain_state, process_row);

        // next action
        if (chain_state.migrating_num_utxos == chain_state.migrated_num_utxos) {
            chain_state.status = deleting_data;
        }
    } else if (chain_state.status == deleting_data) {
        delete_data(chain_state, config.retained_spent_utxo_blocks, config.num_retain_data_blocks, process_row);
    } else if (chain_state.status == distributing_rewards) {
        auto from_index = chain_state.num_validators_assigned;
        auto to_index = from_index + config.num_validators_per_distribution;
        if (to_index > chain_state.num_provider_validators) {
            to_index = chain_state.num_provider_validators;
        }
        chain_state.num_validators_assigned = to_index;

        // distribute rewards to validators in batches
        reward_distribution::endtreward_action _endtreward(REWARD_DISTRIBUTION_CONTRACT, {get_self(), "active"_n});
        _endtreward.send(chain_state.migrating_height, from_index, to_index);

        // check xsat consensus reward
        endorse_manage::consensus_config_table _consensus_config
        = endorse_manage::consensus_config_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
        auto consensus_config = _consensus_config.get_or_default();
        if (consensus_config.version == 2) {
            // check xsat validators size
            reward_distribution::endtreward2_action _endtreward2(REWARD_DISTRIBUTION_CONTRACT, {get_self(), "active"_n});
            _endtreward2.send(chain_state.migrating_height, from_index, to_index);
        }

        if (chain_state.num_provider_validators == chain_state.num_validators_assigned) {
            chain_state.irreversible_height = chain_state.migrating_height;
            chain_state.irreversible_hash = chain_state.migrating_hash;
            chain_state.migrating_height = 0;
            chain_state.migrating_hash = ZERO_HASH;
            chain_state.migrating_num_utxos = 0;
            chain_state.migrated_num_utxos = 0;
            chain_state.num_provider_validators = 0;
            chain_state.num_validators_assigned = 0;
            chain_state.synchronizer = {};
            chain_state.miner = {};
            chain_state.parser = {};
            chain_state.status = parsing;
        }
    } else if (chain_state.status == parsing) {
        parsing_transactions(height, hash, &parsing_progress, process_row);

        if (parsing_progress.num_transactions == parsing_progress.parsed_transactions) {
            auto consensus_block_itr = _consensus_block.require_find(parsing_progress.bucket_id);
            _consensus_block.modify(consensus_block_itr, same_payer, [&](auto& row) {
                row.parse = true;
                row.parser = synchronizer;
                row.num_utxos = parsing_progress.num_utxos;
            });

            chain_state.parsing_progress_of.erase(hash);
        }

        // If all are parsed, set the next parsed block
        if (chain_state.parsing_progress_of.empty()) {
            chain_state.status = waiting;
            chain_state.parsed_height = height;
            chain_state.parsing_height = 0;

            // Set the latest parsable block height
            find_set_next_parsable_block(chain_state, config.parse_timeout_seconds);

            // Set the block height of the latest migration
            find_set_next_irreversible_block(chain_state);
        }
    }

    // save state
    _chain_state.set(chain_state, get_self());

    auto status = get_parsing_status_name(chain_state.status);
    if (parsing_progress.num_transactions > 0
        && parsing_progress.num_transactions == parsing_progress.parsed_transactions) {
        status = "parsing_completed";
    }
    return {.status = status, .height = height, .block_hash = hash};
}

void utxo_manage::parsing_transactions(const uint64_t height, const checksum256& hash,
                                       parsing_progress_row* parsing_progress, uint64_t process_row) {
    auto block_data = block_sync::read_bucket(BLOCK_SYNC_CONTRACT, parsing_progress->bucket_id, BLOCK_CHUNK,
                                              BLOCK_HEADER_SIZE + parsing_progress->parsed_position,
                                              std::numeric_limits<uint64_t>::max());
    eosio::datastream<const char*> block_stream(block_data.data(), block_data.size());

    // init num_transactions
    if (parsing_progress->parsed_position == 0) {
        parsing_progress->num_transactions = bitcoin::varint::decode(block_stream);
    }

    if (process_row == 0)
        process_row = -1;

    uint64_t parsed_position = 0;
    std::vector<uint8_t> script_data = {};
    auto pending_transactions = parsing_progress->num_transactions - parsing_progress->parsed_transactions;
    while (pending_transactions-- && process_row) {
        bitcoin::core::transaction transaction(&block_data);
        block_stream >> transaction;
        auto txid = bitcoin::be_checksum256_from_uint(transaction.merkle_hash());

        // save vin
        for (; parsing_progress->parsed_vin < transaction.inputs.size() && process_row;
             parsing_progress->parsed_vin++, process_row--) {
            auto vin = transaction.inputs[parsing_progress->parsed_vin];
            if (transaction.is_coinbase())
                continue;

            save_pending_utxo(height, hash, bitcoin::be_checksum256_from_uint(vin.previous_output_hash),
                              vin.previous_output_index, script_data, 0, "vin"_n);
            parsing_progress->num_utxos++;
        }

        // save vout
        for (; parsing_progress->parsed_vout < transaction.outputs.size() && process_row;
             parsing_progress->parsed_vout++, process_row--) {
            auto vout = transaction.outputs[parsing_progress->parsed_vout];

            if (xsat::utils::is_unspendable_legacy(vout.script.data))
                continue;
            save_pending_utxo(height, hash, txid, parsing_progress->parsed_vout, vout.script.data, vout.value,
                              "vout"_n);
            parsing_progress->num_utxos++;
        }

        // next transaction
        if (parsing_progress->parsed_vin == transaction.inputs.size()
            && parsing_progress->parsed_vout == transaction.outputs.size()) {
            parsed_position = block_stream.tellp();
            parsing_progress->parsed_vin = 0;
            parsing_progress->parsed_vout = 0;
            parsing_progress->parsed_transactions++;
        }
    }
    parsing_progress->parsed_position += parsed_position;
}

void utxo_manage::migrate(utxo_manage::chain_state_row& chain_state, uint64_t process_row) {
    if (process_row == 0)
        process_row = -1;

    auto block_id = xsat::utils::compute_block_id(chain_state.migrating_height, chain_state.migrating_hash);
    auto pending_utxo_idx = _pending_utxo.get_index<"byblockid"_n>();
    auto start_itr = pending_utxo_idx.lower_bound(block_id);
    auto end_itr = pending_utxo_idx.upper_bound(block_id);

    auto utxo_idx = _utxo.get_index<"byutxoid"_n>();
    while (start_itr != end_itr && process_row--) {
        if (start_itr->type == "vin"_n) {
            auto prev_utxo = remove_utxo(utxo_idx, start_itr->txid, start_itr->index);
            if (prev_utxo.has_value()) {
                chain_state.num_utxos -= 1;

                // migrate to utxo  table
                save_spent_utxo(start_itr->height, *prev_utxo);
            }
        } else {
            save_utxo(start_itr->txid, start_itr->index, start_itr->scriptpubkey, start_itr->value);
            chain_state.num_utxos += 1;
        }

        // erase pending utxo
        start_itr = pending_utxo_idx.erase(start_itr);

        chain_state.migrated_num_utxos++;
    }
}

void utxo_manage::delete_data(utxo_manage::chain_state_row& chain_state, const uint16_t retained_spent_utxo_blocks,
                              const uint16_t num_retain_data_blocks, uint64_t process_row) {
    // Batch delete forked pendingutxos
    auto pending_utxo_idx = _pending_utxo.get_index<"byheight"_n>();
    auto pending_utxo_itr = pending_utxo_idx.lower_bound(chain_state.migrating_height);
    auto pending_utxo_end = pending_utxo_idx.upper_bound(chain_state.migrating_height);
    if (pending_utxo_itr != pending_utxo_end) {
        while (pending_utxo_itr != pending_utxo_end && process_row--) {
            pending_utxo_itr = pending_utxo_idx.erase(pending_utxo_itr);
        }
        return;
    }

    // Delete spentutxos in batches
    auto del_history_height = chain_state.migrating_height - retained_spent_utxo_blocks;
    auto spent_utxo_idx = _spent_utxo.get_index<"byheight"_n>();
    auto spent_utxo_itr = spent_utxo_idx.lower_bound(del_history_height);
    auto spent_utxo_end = spent_utxo_idx.upper_bound(del_history_height);
    if (spent_utxo_itr != spent_utxo_end) {
        while (spent_utxo_itr != spent_utxo_end && process_row--) {
            spent_utxo_itr = spent_utxo_idx.erase(spent_utxo_itr);
        }
        return;
    }

    block_sync::delchunks_action _delchunks(BLOCK_SYNC_CONTRACT, {get_self(), "active"_n});

    // erase endorsement
    block_endorse::erase_action _erase(BLOCK_ENDORSE_CONTRACT, {get_self(), "active"_n});
    _erase.send(chain_state.migrating_height);

    // erase old block chunks
    auto del_height = chain_state.migrating_height - num_retain_data_blocks;
    auto block_extra_itr = _block_extra.find(del_height);
    if (block_extra_itr != _block_extra.end()) {
        _delchunks.send(block_extra_itr->bucket_id);
        _block_extra.erase(block_extra_itr);
    }

    // erase consensus block
    consensus_block_row consensus_block;
    auto consensus_block_idx = _consensus_block.get_index<"byheight"_n>();
    auto consensus_block_itr = consensus_block_idx.lower_bound(chain_state.migrating_height);
    auto consensus_block_end = consensus_block_idx.upper_bound(chain_state.migrating_height);
    while (consensus_block_itr != consensus_block_end) {
        // Without deleting the data of the current latest irreversible block, config.num_retain_data_blocks block
        // data needs to be retained.
        if (consensus_block_itr->hash != chain_state.migrating_hash) {
            _delchunks.send(consensus_block_itr->bucket_id);
        } else {
            consensus_block = *consensus_block_itr;
        }
        consensus_block_itr = consensus_block_idx.erase(consensus_block_itr);
    }

    // save irreversible block
    _block.emplace(get_self(), [&](auto& row) {
        row.height = consensus_block.height;
        row.hash = consensus_block.hash;
        row.cumulative_work = consensus_block.cumulative_work;
        row.version = consensus_block.version;
        row.previous_block_hash = consensus_block.previous_block_hash;
        row.merkle = consensus_block.merkle;
        row.timestamp = consensus_block.timestamp;
        row.bits = consensus_block.bits;
        row.nonce = consensus_block.nonce;
    });

    // save block extra
    _block_extra.emplace(get_self(), [&](auto& row) {
        row.height = chain_state.migrating_height;
        row.bucket_id = consensus_block.bucket_id;
    });

    // next action
    chain_state.status = distributing_rewards;
}

void utxo_manage::find_set_next_parsable_block(utxo_manage::chain_state_row& chain_state,
                                               const uint16_t parse_timeout_seconds) {
    if (chain_state.parsing_height != 0)
        return;
    uint128_t id = compute_parse_height(false, chain_state.irreversible_height);
    auto block_id_idx = _consensus_block.get_index<"parseheight"_n>();
    auto consensus_block_itr = block_id_idx.upper_bound(id);
    while (consensus_block_itr != block_id_idx.end()) {
        // break if the latest one is parsed
        if (consensus_block_itr->parse)
            break;

        if (chain_state.parsing_height == 0) {
            chain_state.parsing_height = consensus_block_itr->height;
            // If the next height is different from the previous one, break
        } else if (chain_state.parsing_height != consensus_block_itr->height) {
            break;
        }
        chain_state.parsing_progress_of[consensus_block_itr->hash]
            = {.bucket_id = consensus_block_itr->bucket_id,
               .parser = consensus_block_itr->synchronizer,
               .parse_expiration_time = current_time_point() + eosio::seconds(parse_timeout_seconds)};
        consensus_block_itr++;
    }
}

void utxo_manage::find_set_next_irreversible_block(utxo_manage::chain_state_row& chain_state) {
    // Verify whether the next irreversible block needs to be migrated
    if (chain_state.migrating_height != 0
        || chain_state.parsing_height <= chain_state.irreversible_height + IRREVERSIBLE_BLOCKS) {
        return;
    }

    // Get the next irreversible block
    auto consensus_block = find_next_irreversible_block(chain_state.irreversible_height, chain_state.irreversible_hash);
    chain_state.migrating_height = consensus_block.height;
    chain_state.migrating_hash = consensus_block.hash;
    chain_state.miner = consensus_block.miner;
    chain_state.synchronizer = consensus_block.synchronizer;
    chain_state.parser = consensus_block.parser;
    chain_state.migrating_num_utxos = consensus_block.num_utxos;

    // Set the number of endorsed users
    block_endorse::endorsement_table _endorsement(BLOCK_ENDORSE_CONTRACT, chain_state.migrating_height);
    auto endorsement_idx = _endorsement.get_index<"byhash"_n>();
    auto endorsement_itr = endorsement_idx.require_find(chain_state.migrating_hash);

    endorse_manage::consensus_config_table _consensus_config
        = endorse_manage::consensus_config_table(ENDORSER_MANAGE_CONTRACT, ENDORSER_MANAGE_CONTRACT.value);
    auto consensus_config = _consensus_config.get_or_default();
    if (consensus_config.version == 2) {
        // check xsat validators size
        // XSAT endorsement: scope = migrating height | 0x100000000
        constexpr uint64_t XSAT_SCOPE_MASK = 0x100000000;   
        block_endorse::endorsement_table endorsement_xsat(BLOCK_ENDORSE_CONTRACT, chain_state.migrating_height | XSAT_SCOPE_MASK);
        auto endorsement_idx_xsat = endorsement_xsat.get_index<"byhash"_n>();
        auto itr_xsat = endorsement_idx_xsat.require_find(chain_state.migrating_hash);
        chain_state.num_provider_validators = std::max(endorsement_itr->provider_validators.size(), 
                                                     itr_xsat->provider_validators.size());
    } else {
        chain_state.num_provider_validators = endorsement_itr->provider_validators.size();
    }

    auto consensus_block_itr = _consensus_block.find(consensus_block.bucket_id);
    _consensus_block.modify(consensus_block_itr, same_payer, [&](auto& row) {
        row.irreversible = true;
    });
}

utxo_manage::consensus_block_row utxo_manage::find_next_irreversible_block(const uint64_t irreversible_height,
                                                                           const checksum256& irreversible_hash) {
    const auto err_msg = "4006:utxomng.xsat::processblock: next irreversible block not found";
    // If the next block does not fork, return directly
    auto height_idx = _consensus_block.get_index<"byheight"_n>();
    auto next_irreversible_itr = height_idx.lower_bound(irreversible_height + 1);
    auto next_irreversible_end = height_idx.upper_bound(irreversible_height + 1);
    auto block_size = std::distance(next_irreversible_itr, next_irreversible_end);
    check(block_size > 0, err_msg);
    if (block_size == 1) {
        check(next_irreversible_itr->previous_block_hash == irreversible_hash, err_msg);
        return *next_irreversible_itr;
    }

    // Find the parent block with the largest cumulative work after 6 blocks
    consensus_block_row parent;
    auto parent_height = irreversible_height + IRREVERSIBLE_BLOCKS;
    auto parent_itr = height_idx.lower_bound(parent_height);
    auto parent_end = height_idx.upper_bound(parent_height);
    check(parent_itr != parent_end, err_msg);
    while (parent_itr != parent_end) {
        if (parent.cumulative_work < parent_itr->cumulative_work) {
            parent = *parent_itr;
        }
        parent_itr++;
    }

    // Backtrack from the parent block to the irreversible block
    auto block_id_idx = _consensus_block.get_index<"byblockid"_n>();
    auto irreversible_block = block_id_idx.require_find(
        xsat::utils::compute_block_id(parent.height - 1, parent.previous_block_hash), err_msg);
    while (irreversible_block->previous_block_hash != irreversible_hash) {
        irreversible_block = block_id_idx.require_find(
            xsat::utils::compute_block_id(irreversible_block->height - 1, irreversible_block->previous_block_hash),
            err_msg);
    }
    check(irreversible_block->previous_block_hash == irreversible_hash, err_msg);
    return *irreversible_block;
}

void utxo_manage::save_spent_utxo(const uint64_t height, const utxo_manage::utxo_row& utxo) {
    auto id = _spent_utxo.available_primary_key();
    if (id == 0) {
        id = 1;
    }
    _spent_utxo.emplace(get_self(), [&](auto& row) {
        row.id = id;
        row.height = height;
        row.txid = utxo.txid;
        row.index = utxo.index;
        row.scriptpubkey = utxo.scriptpubkey;
        row.value = utxo.value;
    });
}

void utxo_manage::save_pending_utxo(const uint64_t height, const checksum256& hash, const checksum256& txid,
                                    const uint32_t index, const std::vector<uint8_t>& script_data, const uint64_t value,
                                    const name& type) {
    auto id = _pending_utxo.available_primary_key();
    if (id == 0) {
        id = 1;
    }
    _pending_utxo.emplace(get_self(), [&](auto& row) {
        row.id = id;
        row.height = height;
        row.hash = hash;
        row.txid = txid;
        row.index = index;
        row.scriptpubkey = script_data;
        row.value = value;
        row.type = type;
    });
}

utxo_manage::utxo_row utxo_manage::save_utxo(const checksum256& txid, const uint32_t index,
                                             const std::vector<uint8_t>& script_data, const uint64_t value) {
    //  save output
    auto id = _utxo.available_primary_key();
    if (id == 0) {
        id = 1;
    }
    auto utxo_itr = _utxo.emplace(get_self(), [&](auto& row) {
        row.id = id;
        row.txid = txid;
        row.index = index;
        row.scriptpubkey = script_data;
        row.value = value;
    });
    return *utxo_itr;
}

template <typename IDX>
optional<utxo_manage::utxo_row> utxo_manage::remove_utxo(IDX& utxo_idx, const checksum256& prev_txid,
                                                         const uint32_t prev_index) {
    auto utxo_itr = utxo_idx.find(xsat::utils::compute_utxo_id(prev_txid, prev_index));
    if (utxo_itr != utxo_idx.end()) {
        auto found_utxo = *utxo_itr;
        utxo_idx.erase(utxo_itr);
        return found_utxo;
    } else {
        // log
        utxo_manage::lostutxolog_action _lostutxolog(get_self(), {get_self(), "active"_n});
        _lostutxolog.send(prev_txid, prev_index);
        return nullopt;
    }
}
