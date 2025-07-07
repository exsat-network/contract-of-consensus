#include <blksync.xsat/blksync.xsat.hpp>
#include <poolreg.xsat/poolreg.xsat.hpp>
#include <rescmng.xsat/rescmng.xsat.hpp>
#include <utxomng.xsat/utxomng.xsat.hpp>
#include <bitcoin/script/address.hpp>
#include <bitcoin/core/block_header.hpp>
#include <bitcoin/core/pow.hpp>
#include <cmath>
#include "../internal/defines.hpp"

#ifdef DEBUG
#include "./src/debug.hpp"
#endif

// @auth utxomng.xsat
[[eosio::action]]
void block_sync::consensus(const uint64_t height, const name& synchronizer, const uint64_t bucket_id) {
    require_auth(UTXO_MANAGE_CONTRACT);

    block_bucket_table _block_bucket = block_bucket_table(get_self(), synchronizer.value);
    auto block_bucket_itr
        = _block_bucket.require_find(bucket_id, "blksync.xsat::consensus: block bucket does not exists");
    auto hash = block_bucket_itr->hash;
    _block_bucket.erase(block_bucket_itr);

    // erase passed index
    passed_index_table _passed_index(get_self(), height);
    auto passed_index_idx = _passed_index.get_index<"byhash"_n>();
    auto passed_index_itr = passed_index_idx.lower_bound(hash);
    auto passed_index_end = passed_index_idx.upper_bound(hash);
    while (passed_index_itr != passed_index_end) {
        passed_index_itr = passed_index_idx.erase(passed_index_itr);
    }

    // erase block miner
    block_miner_table _block_miner(get_self(), height);
    auto block_miner_idx = _block_miner.get_index<"byhash"_n>();
    auto block_miner_itr = block_miner_idx.lower_bound(hash);
    auto block_miner_end = block_miner_idx.upper_bound(hash);
    if (block_miner_itr != block_miner_end) {
        block_miner_idx.erase(block_miner_itr);
    }
}

//@auth utxomng.xsat
[[eosio::action]]
void block_sync::delchunks(const uint64_t bucket_id) {
    require_auth(UTXO_MANAGE_CONTRACT);

    // erase block.chunk
    auto iter = eosio::internal_use_do_not_use::db_lowerbound_i64(get_self().value, bucket_id, BLOCK_CHUNK.value, 0);
    while (iter >= 0) {
        uint64_t ignored;
        auto next_iter = eosio::internal_use_do_not_use::db_next_i64(iter, &ignored);
        eosio::internal_use_do_not_use::db_remove_i64(iter);
        iter = next_iter;
    }
}

//@auth synchronizer
[[eosio::action]]
void block_sync::initbucket(const name& synchronizer, const uint64_t height, const checksum256& hash,
                            const uint32_t block_size, const uint8_t num_chunks, const uint32_t chunk_size) {
    require_auth(synchronizer);

    check(height > START_HEIGHT, "2001:blksync.xsat::initbucket: height must be greater than 840000");
    check(block_size > BLOCK_HEADER_SIZE && block_size <= MAX_BLOCK_SIZE,
          "2002:blksync.xsat::initbucket: block_size must be greater than 80 and less than or equal to 4194304");
    check(num_chunks > 0 && num_chunks <= MAX_NUM_CHUNKS,
          "2003:blksync.xsat::initbucket: num_chunks must be greater than 0 and less than or equal to 64");

    // check whether it is a synchronizer
    pool::synchronizer_table _synchronizer(POOL_REGISTER_CONTRACT, POOL_REGISTER_CONTRACT.value);
    auto synchronizer_itr
        = _synchronizer.require_find(synchronizer.value, "2004:blksync.xsat::initbucket: not an synchronizer account");

    check(!utxo_manage::check_consensus(height, hash),
          "2005:blksync.xsat::initbucket: the block has reached consensus");

    // fee deduction
    resource_management::pay_action pay(RESOURCE_MANAGE_CONTRACT, {get_self(), "active"_n});
    pay.send(height, hash, synchronizer, PUSH_CHUNK, 1);

    block_bucket_table _block_bucket = block_bucket_table(get_self(), synchronizer.value);
    auto block_bucket_idx = _block_bucket.get_index<"byblockid"_n>();
    auto block_bucket_itr = block_bucket_idx.find(xsat::utils::compute_block_id(height, hash));

    uint64_t bucket_id = 0;
    if (block_bucket_itr == block_bucket_idx.end()) {
        // slot limit
        check(synchronizer_itr->produced_block_limit == 0
                  || height <= synchronizer_itr->produced_block_limit + synchronizer_itr->latest_produced_block_height ,
              "2006:blksync.xsat::initbucket: to become a synchronizer, a block must be produced within 72 hours");
        const auto num_slots = std::distance(_block_bucket.begin(), _block_bucket.end());
        check(num_slots < synchronizer_itr->num_slots,
              "2007:blksync.xsat::initbucket: not enough slots, please buy more slots");

        bucket_id = next_bucket_id();
        _block_bucket.emplace(get_self(), [&](auto& row) {
            row.bucket_id = bucket_id;
            row.height = height;
            row.hash = hash;
            row.size = block_size;
            row.num_chunks = num_chunks;
            row.chunk_size = chunk_size;
            row.status = uploading;
            row.updated_at = current_time_point();
        });
    } else {
        check(block_bucket_itr->status == uploading || block_bucket_itr->status == verify_fail,
              "2008:blksync.xsat::initbucket: cannot init bucket in the current state ["
                  + get_block_status_name(block_bucket_itr->status) + "]");

        bucket_id = block_bucket_itr->bucket_id;
        block_bucket_idx.modify(block_bucket_itr, same_payer, [&](auto& row) {
            row.size = block_size;
            row.num_chunks = num_chunks;
            row.chunk_size = chunk_size;
            row.status = uploading;
            row.updated_at = current_time_point();
        });
    }

    // log
    block_sync::bucketlog_action _bucketlog(get_self(), {get_self(), "active"_n});
    _bucketlog.send(bucket_id, synchronizer, height, hash, block_size, num_chunks, chunk_size);
}

//@auth synchronizer
[[eosio::action]]
void block_sync::pushchunk(const name& synchronizer, const uint64_t height, const checksum256& hash,
                           const uint8_t chunk_id, const eosio::ignore<std::vector<char>>& data) {
    require_auth(synchronizer);

    check(!utxo_manage::check_consensus(height, hash), "2009:blksync.xsat::pushchunk: the block has reached consensus");

    // check
    eosio::unsigned_int size;
    _ds >> size;
    auto data_size = _ds.remaining();
    check(data_size == (size_t)size, "2010:blksync.xsat::pushchunk: data size does not match");
    check(data_size > 0, "2011:blksync.xsat::pushchunk: data size must be greater than 0");

    block_bucket_table _block_bucket = block_bucket_table(get_self(), synchronizer.value);
    auto block_bucket_idx = _block_bucket.get_index<"byblockid"_n>();
    auto block_bucket_itr = block_bucket_idx.require_find(
        xsat::utils::compute_block_id(height, hash), "2012:blksync.xsat::pushchunk: [blockbuckets] does not exists");

    auto status = block_bucket_itr->status;
    check(
        status == uploading || status == upload_complete || status == verify_fail,
        "2013:blksync.xsat::pushchunk: cannot push chunk in the current state [" + get_block_status_name(status) + "]");

    // fee deduction
    resource_management::pay_action pay(RESOURCE_MANAGE_CONTRACT, {get_self(), "active"_n});
    pay.send(height, hash, synchronizer, PUSH_CHUNK, 1);

    uint32_t pre_size = 0;
    // emplace/modify chunk
    auto bucket_id = block_bucket_itr->bucket_id;
    auto chunk_itr
        = eosio::internal_use_do_not_use::db_find_i64(get_self().value, bucket_id, BLOCK_CHUNK.value, chunk_id);
    if (chunk_itr >= 0) {
        pre_size = eosio::internal_use_do_not_use::db_get_i64(chunk_itr, nullptr, 0);
        eosio::internal_use_do_not_use::db_update_i64(chunk_itr, get_self().value, _ds.pos(), data_size);
    } else {
        eosio::internal_use_do_not_use::db_store_i64(bucket_id, BLOCK_CHUNK.value, get_self().value, chunk_id,
                                                     _ds.pos(), data_size);
    }
    block_bucket_idx.modify(block_bucket_itr, same_payer, [&](auto& row) {
        if (chunk_itr < 0) {
            row.uploaded_num_chunks += 1;
            row.chunk_ids.insert(chunk_id);
        }
        row.uploaded_size = row.uploaded_size + data_size - pre_size;

        check(row.uploaded_num_chunks <= row.num_chunks,
              "2023:blksync.xsat::pushchunk: the number of uploaded chunks has exceeded [num_chunks]");
        check(row.uploaded_size <= row.size,
              "2024:blksync.xsat::pushchunk: the upload chunk data size has exceeded [size]");

        if (row.uploaded_size == row.size && row.uploaded_num_chunks == row.num_chunks) {
            row.status = upload_complete;
        } else {
            row.status = uploading;
        }
        row.updated_at = current_time_point();
    });

    // log
    block_sync::chunklog_action _chunklog(get_self(), {get_self(), "active"_n});
    _chunklog.send(block_bucket_itr->bucket_id, chunk_id, block_bucket_itr->uploaded_num_chunks);
}

//@auth synchronizer
[[eosio::action]]
void block_sync::delchunk(const name& synchronizer, const uint64_t height, const checksum256& hash,
                          const uint8_t chunk_id) {
    require_auth(synchronizer);

    // check
    block_bucket_table _block_bucket = block_bucket_table(get_self(), synchronizer.value);
    auto block_bucket_idx = _block_bucket.get_index<"byblockid"_n>();
    auto block_bucket_itr = block_bucket_idx.require_find(
        xsat::utils::compute_block_id(height, hash), "2014:blksync.xsat::delchunk: [blockbuckets] does not exists");

    auto status = block_bucket_itr->status;
    check(status == uploading || status == upload_complete || status == verify_fail,
          "2015:blksync.xsat::delchunk: cannot delete chunk in the current state [" + get_block_status_name(status)
              + "]");

    // fee deduction
    resource_management::pay_action pay(RESOURCE_MANAGE_CONTRACT, {get_self(), "active"_n});
    pay.send(height, hash, synchronizer, PUSH_CHUNK, 1);

    auto bucket_id = block_bucket_itr->bucket_id;

    auto chunk_itr
        = eosio::internal_use_do_not_use::db_find_i64(get_self().value, bucket_id, BLOCK_CHUNK.value, chunk_id);
    check(chunk_itr >= 0, "2016:blksync.xsat::delchunk: chunk_id does not exist");

    auto chunk_size = eosio::internal_use_do_not_use::db_get_i64(chunk_itr, nullptr, 0);
    eosio::internal_use_do_not_use::db_remove_i64(chunk_itr);

    block_bucket_idx.modify(block_bucket_itr, same_payer, [&](auto& row) {
        row.uploaded_num_chunks -= 1;
        row.uploaded_size -= chunk_size;
        row.chunk_ids.erase(chunk_id);
        row.updated_at = current_time_point();

        if (row.uploaded_size == row.size && row.uploaded_num_chunks == row.num_chunks) {
            row.status = upload_complete;
        } else {
            row.status = uploading;
        }
    });

    // log
    block_sync::delchunklog_action _delchunklog(get_self(), {get_self(), "active"_n});
    _delchunklog.send(bucket_id, chunk_id, block_bucket_itr->uploaded_num_chunks);
}

//@auth synchronizer
[[eosio::action]]
void block_sync::delbucket(const name& synchronizer, const uint64_t height, const checksum256& hash) {
    if (!has_auth(get_self())) {
        require_auth(synchronizer);
    }

    // check
    block_bucket_table _block_bucket = block_bucket_table(get_self(), synchronizer.value);
    auto block_bucket_idx = _block_bucket.get_index<"byblockid"_n>();
    auto block_bucket_itr = block_bucket_idx.require_find(
        xsat::utils::compute_block_id(height, hash), "2017:blksync.xsat::delbucket: [blockbuckets] does not exists");

    // fee deduction
    resource_management::pay_action pay(RESOURCE_MANAGE_CONTRACT, {get_self(), "active"_n});
    pay.send(height, hash, synchronizer, PUSH_CHUNK, 1);

    auto block_hash = block_bucket_itr->hash;
    auto bucket_id = block_bucket_itr->bucket_id;

    // erase block.chunk
    auto iter = eosio::internal_use_do_not_use::db_lowerbound_i64(get_self().value, bucket_id, BLOCK_CHUNK.value, 0);
    while (iter >= 0) {
        uint64_t ignored;
        auto next_iter = eosio::internal_use_do_not_use::db_next_i64(iter, &ignored);
        eosio::internal_use_do_not_use::db_remove_i64(iter);
        iter = next_iter;
    }

    // erase block bucket
    block_bucket_idx.erase(block_bucket_itr);

    // erase passed index
    passed_index_table _passed_index(get_self(), height);
    auto passed_index_idx = _passed_index.get_index<"byhash"_n>();
    auto passed_index_itr = passed_index_idx.lower_bound(block_hash);
    auto passed_index_end = passed_index_idx.upper_bound(block_hash);
    while (passed_index_itr != passed_index_end) {
        if (passed_index_itr->synchronizer == synchronizer) {
            passed_index_itr = passed_index_idx.erase(passed_index_itr);
        } else {
            passed_index_itr++;
        }
    }

    // log
    block_sync::delbucketlog_action _delbucketlog(get_self(), {get_self(), "active"_n});
    _delbucketlog.send(bucket_id);
}

//@auth synchronizer
[[eosio::action]]
block_sync::verify_block_result block_sync::verify(const name& synchronizer, const uint64_t height,
                                                   const checksum256& hash, const uint64_t nonce) {
    require_auth(synchronizer);

    block_bucket_table _block_bucket = block_bucket_table(get_self(), synchronizer.value);
    auto block_bucket_idx = _block_bucket.get_index<"byblockid"_n>();
    auto block_bucket_itr = block_bucket_idx.require_find(
        xsat::utils::compute_block_id(height, hash),
        "2018:blksync.xsat::verify: you have not uploaded the block data. please upload it first and then verify it");
    check(block_bucket_itr->in_verifiable(), "2019:blksync.xsat::verify: cannot validate block in the current state ["
                                                 + get_block_status_name(block_bucket_itr->status) + "]");

    // fee deduction
    resource_management::pay_action pay(RESOURCE_MANAGE_CONTRACT, {get_self(), "active"_n});
    pay.send(height, hash, synchronizer, VERIFY, 1);
    auto status = block_bucket_itr->status;

    if (utxo_manage::check_consensus(height, hash)) {
        return check_fail(block_bucket_idx, block_bucket_itr, "reached_consensus", hash);
    }

    auto verify_info = block_bucket_itr->verify_info.value_or(verify_info_data{});
    if (status == upload_complete || status == verify_merkle) {
        // check merkle
        auto error_msg = check_merkle(block_bucket_itr, verify_info);
        if (error_msg.has_value()) {
            return check_fail(block_bucket_idx, block_bucket_itr, *error_msg, hash);
        }

        // next action
        if (verify_info.num_transactions == verify_info.processed_transactions
            && verify_info.processed_position == block_bucket_itr->size) {
            status = verify_parent_hash;
        } else {
            status = verify_merkle;
        }

        // update block status
        block_bucket_idx.modify(block_bucket_itr, same_payer, [&](auto& row) {
            row.status = status;
            row.verify_info = verify_info;
            row.updated_at = current_time_point();
        });
        return {.status = get_block_status_name(status), .block_hash = hash};
    }

    if (status == verify_parent_hash) {
        // check parent
        auto parent_block = utxo_manage::get_ancestor(height - 1, verify_info.previous_block_hash);
        check(parent_block.has_value(), "2020:blksync.xsat::verify: parent block hash did not reach consensus");

        checksum256 cumulative_work
            = bitcoin::be_checksum256_from_uint(bitcoin::be_uint_from_checksum256(verify_info.work)
                                                + bitcoin::be_uint_from_checksum256(parent_block->cumulative_work));

        auto expected_bits = bitcoin::core::get_next_work_required(*parent_block, verify_info.timestamp,
                                                                   utxo_manage::get_ancestor, CHAIN_PARAMS);
        if (verify_info.bits != expected_bits) {
            return check_fail(block_bucket_idx, block_bucket_itr, "bad-diffbits", hash);
        }

        auto miner = verify_info.miner;

        // save the miner information that passed for the first time
        block_miner_table _block_miner(get_self(), height);
        auto block_miner_idx = _block_miner.get_index<"byhash"_n>();
        auto block_miner_itr = block_miner_idx.find(hash);
        uint32_t expired_block_num = 0;
        uint64_t block_id = 0;
        if (block_miner_itr == block_miner_idx.end()) {
            if (miner) {
                utxo_manage::config_table _config(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
                auto config = _config.get();
                expired_block_num = current_block_number() + config.num_miner_priority_blocks;
            }
            block_id = _block_miner.available_primary_key();
            _block_miner.emplace(get_self(), [&](auto& row) {
                row.id = block_id;
                row.hash = hash;
                row.miner = miner;
                row.expired_block_num = expired_block_num;
            });
        } else {
            expired_block_num = block_miner_itr->expired_block_num;
            block_id = block_miner_itr->id;
        }

        // If it is a miner and has not exceeded expired block_num, it is 0, otherwise it is 1
        uint64_t miner_priority = 1;
        if (miner == synchronizer && expired_block_num > current_block_number()) {
            miner_priority = 0;
        }

        passed_index_table _passed_index(get_self(), height);
        auto passed_index_itr = _passed_index.lower_bound(compute_passed_index_id(block_id, 0, 0));
        auto passed_index_end = _passed_index.upper_bound(compute_passed_index_id(block_id, 1, MAX_UINT_24));
        auto has_passed_index = passed_index_itr != passed_index_end;
        auto last_passed_index = has_passed_index ? --passed_index_end : passed_index_end;

        uint64_t pass_number = 1;
        if (has_passed_index) {
            pass_number = (last_passed_index->id & 0xFFFFFF) + 1;
        }

        uint64_t passed_index_id = compute_passed_index_id(block_id, miner_priority, pass_number);

        // save passed index
        _passed_index.emplace(get_self(), [&](auto& row) {
            row.id = passed_index_id;
            row.hash = hash;
            row.bucket_id = block_bucket_itr->bucket_id;
            row.synchronizer = synchronizer;
            row.miner = miner;
            row.cumulative_work = cumulative_work;
            row.created_at = current_time_point();
        });

        status = verify_pass;
        if (synchronizer != miner && expired_block_num > current_block_number()) {
            status = waiting_miner_verification;
        }

        if (status == verify_pass) {
            utxo_manage::consensus_action _consensus(UTXO_MANAGE_CONTRACT, {get_self(), "active"_n});
            _consensus.send(height, hash);
        }

        // update block status
        block_bucket_idx.modify(block_bucket_itr, same_payer, [&](auto& row) {
            row.status = status;
            row.updated_at = current_time_point();
        });
        return {.status = get_block_status_name(status), .block_hash = hash};
    }

    // waiting_miner_verification
    block_miner_table _block_miner(get_self(), height);
    auto block_miner_idx = _block_miner.get_index<"byhash"_n>();
    auto block_miner_itr
        = block_miner_idx.require_find(hash, "2021:blksync.xsat::verify: [blockminer] does not exists");
    check(block_miner_itr->expired_block_num <= current_block_number(),
          "2022:blksync.xsat::verify: waiting for miners to produce blocks");

    utxo_manage::consensus_action _consensus(UTXO_MANAGE_CONTRACT, {get_self(), "active"_n});
    _consensus.send(height, hash);

    block_bucket_idx.modify(block_bucket_itr, same_payer, [&](auto& row) {
        row.status = verify_pass;
        row.updated_at = current_time_point();
    });
    return {.status = get_block_status_name(verify_pass), .block_hash = hash};
}

optional<string> block_sync::check_transaction(const bitcoin::core::transaction tx) {
    if (tx.inputs.empty()) {
        return "bad-txns-vin-empty";
    }

    if (tx.outputs.empty()) {
        return "bad-txns-vout-empty";
    }

    uint64_t value_out = 0;
    for (const auto& output : tx.outputs) {
        if (output.value > BTC_SUPPLY) {
            return "bad-txns-vout-toolarge";
        }
        value_out += output.value;
        if (value_out > BTC_SUPPLY) {
            return "bad-txns-txouttotal-toolarge";
        }
    }
    auto is_coinbase = tx.is_coinbase();
    std::set<out_point> vin_out_points;
    for (const auto& input : tx.inputs) {
        if (!vin_out_points.insert(out_point{.tx_id = input.previous_output_hash, .index = input.previous_output_index})
                 .second) {
            return "bad-txns-inputs-duplicate";
        }
        if (is_coinbase) {
            auto script_size = tx.inputs[0].script_sig.data.size();
            if (script_size < 2 || script_size > 100) {
                return "bad-cb-length";
            }
        } else {
            if (input.previous_output_hash == bitcoin::uint256_t(0)) {
                return "bad-txns-prevout-null";
            }
        }
    }
}

//@private
template <typename ITR>
optional<string> block_sync::check_merkle(const ITR& block_bucket_itr, verify_info_data& verify_info) {
    const auto block_size = block_bucket_itr->size;
    const auto bucket_id = block_bucket_itr->bucket_id;
    const auto height = block_bucket_itr->height;

    auto block_data = read_bucket(get_self(), bucket_id, BLOCK_CHUNK, verify_info.processed_position, block_size);
    eosio::datastream<const char*> block_stream(block_data.data(), block_data.size());

    auto hash = block_bucket_itr->hash;
    // verify header
    if (verify_info.processed_position == 0) {
        bitcoin::core::block_header block_header;
        block_stream >> block_header;

        auto block_hash = bitcoin::be_checksum256_from_uint(block_header.hash());

        if (block_hash != hash) {
            return "hash_mismatch";
        }

        if (block_header.version_are_invalid(height, CHAIN_PARAMS)) {
            return "bad-version";
        }

        if (!block_header.target_is_valid()) {
            return "invalid_target";
        }

        verify_info.num_transactions = bitcoin::varint::decode(block_stream);
        // check transactions size
        if (verify_info.num_transactions == 0) {
            return "tx_size_limits";
        }

        verify_info.previous_block_hash = bitcoin::be_checksum256_from_uint(block_header.previous_block_hash);
        verify_info.work = bitcoin::be_checksum256_from_uint(block_header.work());
        verify_info.header_merkle = bitcoin::le_checksum256_from_uint(block_header.merkle);
        verify_info.timestamp = block_header.timestamp;
        verify_info.bits = block_header.bits;
    }

    // deserialization transaction
    utxo_manage::config_table _config(UTXO_MANAGE_CONTRACT, UTXO_MANAGE_CONTRACT.value);
    auto config = _config.get();
    auto num_txs_per_verification = config.num_txs_per_verification;
    auto pending_transactions = verify_info.num_transactions - verify_info.processed_transactions;
    auto rows = num_txs_per_verification;
    if (rows > pending_transactions) {
        rows = pending_transactions;
    }
    std::vector<bitcoin::core::transaction> transactions;
    transactions.reserve(rows);
    for (auto i = 0; i < rows; i++) {
        // Coinbase needs to obtain witness data
        bool allow_witness = verify_info.processed_position == 0 && i == 0;
        bitcoin::core::transaction transaction(&block_data, allow_witness);
        block_stream >> transaction;

        auto err_msg = check_transaction(transaction);
        if (err_msg.has_value()) {
            return err_msg;
        }

        transactions.emplace_back(std::move(transaction));
    }

    if (!verify_info.has_witness) {
        verify_info.has_witness = std::any_of(transactions.cbegin(), transactions.cend(), [](const auto& trx) {
            return trx.witness.size() > 0;
        });
    }

    // check witness ?
    if (verify_info.processed_position == 0 && transactions.front().inputs.size() > 0) {
        if (!transactions.front().is_coinbase()) {
            return "coinbase_missing";
        }
        const auto& cbtrx = transactions.front();
        verify_info.witness_reserve_value = cbtrx.get_witness_reserve_value();
        if (cbtrx.witness.size() > 0 && !verify_info.witness_reserve_value.has_value()) {
            return "bad-witness-nonce-size";
        }

        verify_info.witness_commitment = cbtrx.get_witness_commitment();

        find_miner(cbtrx.outputs, verify_info.miner, verify_info.btc_miners);
    }

    auto need_witness_check
        = verify_info.witness_reserve_value.has_value() && verify_info.witness_commitment.has_value();

    // obtain relay merkle
    bool mutated;
    bitcoin::uint256_t header_merkle = bitcoin::core::generate_header_merkle(transactions, &mutated);
    if (mutated) {
        return "bad_txns_duplicate";
    }

    bitcoin::uint256_t witness_merkle;
    if (need_witness_check) {
        witness_merkle = bitcoin::core::generate_witness_merkle(transactions, /*mutated=*/nullptr);
    }

    // calculate to the same layer
    if (verify_info.num_transactions > num_txs_per_verification && rows != num_txs_per_verification) {
        uint8_t current_layer = static_cast<uint8_t>(std::ceil(std::log2(rows)));
        vector<bitcoin::uint256_t> cur_hashes;
        cur_hashes.reserve(2);
        for (; current_layer < config.num_merkle_layer; current_layer++) {
            cur_hashes = {header_merkle, header_merkle};
            header_merkle = bitcoin::generate_merkle_root(cur_hashes);
            if (need_witness_check) {
                cur_hashes = {witness_merkle, witness_merkle};
                witness_merkle = bitcoin::generate_merkle_root(cur_hashes);
            }
        }
    }

    // save sub merkle
    verify_info.relay_header_merkle.emplace_back(bitcoin::le_checksum256_from_uint(header_merkle));
    if (need_witness_check) {
        verify_info.relay_witness_merkle.emplace_back(bitcoin::le_checksum256_from_uint(witness_merkle));
    }

    // save processed position
    verify_info.processed_transactions += rows;
    verify_info.processed_position += block_stream.tellp();

    // check data size
    if (verify_info.num_transactions == verify_info.processed_transactions
        && verify_info.processed_position < block_size) {
        return "data_exceeds";
    }

    if (verify_info.processed_position == block_size
        && verify_info.num_transactions > verify_info.processed_transactions) {
        return "missing_block_data";
    }

    // verify merkle
    if (verify_info.num_transactions == verify_info.processed_transactions
        && verify_info.processed_position == block_size) {
        // verify header merkle
        auto header_merkle_root = bitcoin::generate_merkle_root(verify_info.relay_header_merkle);
        if (header_merkle_root != bitcoin::le_uint_from_checksum256(verify_info.header_merkle)) {
            return "merkle_invalid";
        }

        // verify witness merkle
        if (need_witness_check) {
            auto witness_merkle_root = bitcoin::core::generate_witness_merkle(verify_info.relay_witness_merkle,
                                                                              *verify_info.witness_reserve_value);
            if (witness_merkle_root != bitcoin::le_uint_from_checksum256(*verify_info.witness_commitment)) {
                return "witness_merkle_invalid";
            }
        } else if (verify_info.has_witness) {
            return "witness_merkle_invalid";
        }
    }
    return std::nullopt;
}
//@private
template <typename T, typename ITR>
block_sync::verify_block_result block_sync::check_fail(T& _block_bucket, const ITR block_bucket_itr,
                                                       const string& state, const checksum256& block_hash) {
    _block_bucket.modify(block_bucket_itr, same_payer, [&](auto& row) {
        row.status = verify_fail;
        row.reason = state;
        row.updated_at = current_time_point();
        row.verify_info = std::nullopt;
    });
    return block_sync::verify_block_result{
        .status = get_block_status_name(verify_fail),
        .reason = state,
        .block_hash = block_hash,
    };
}

void block_sync::find_miner(std::vector<bitcoin::core::transaction_output> outputs, name& miner,
                            vector<string>& btc_miners) {
    pool::miner_table _miner = pool::miner_table(POOL_REGISTER_CONTRACT, POOL_REGISTER_CONTRACT.value);
    auto miner_idx = _miner.get_index<"byminer"_n>();
    for (const auto output : outputs) {
        if (!miner) {
            miner = xsat::utils::get_op_return_eos_account(output.script.data);
        }

        std::vector<string> to;
        bitcoin::ExtractDestination(output.script.data, CHAIN_PARAMS, to);
        if (to.size() == 1) {
            btc_miners.push_back(to[0]);
            if (!miner) {
                auto miner_itr = miner_idx.find(xsat::utils::hash(to[0]));
                if (miner_itr != miner_idx.end()) {
                    miner = miner_itr->synchronizer;
                }
            }
        }
    }
    if (!miner)
        btc_miners.clear();
}

uint64_t block_sync::next_bucket_id() {
    global_id_row global_id = _global_id.get_or_default();
    global_id.bucket_id++;
    _global_id.set(global_id, get_self());
    return global_id.bucket_id;
}
