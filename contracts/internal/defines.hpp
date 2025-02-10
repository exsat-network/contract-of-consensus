#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <bitcoin/core/chain.hpp>

using namespace eosio;

// CHAIN PARAMS
#if defined(TESTNET)
static const bitcoin::core::Params CHAIN_PARAMS
    = {.BIP34_height = 227931,
       .BIP65_height = 388381,   // 000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0
       .BIP66_height = 363725,   // 00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931
       .CSV_height = 419328,     // 000000000000000004a1b34462cb8aeebd5799177f7a29cf28f2d1961716b5b5
       .Segwit_height = 481824,  // 0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893
       .pow_limit = "0x00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
       .pow_target_timespan = 14 * 24 * 60 * 60,
       .pow_target_spacing = 10 * 60,
       .pow_allow_min_difficulty_blocks = true,
       .enforce_BIP94 = false,
       .bech32_hrp = "tb",
       .base58Prefixes = {
           std::vector<unsigned char>(1, 111),  // PUBKEY_ADDRESS
           std::vector<unsigned char>(1, 196),  // SCRIPT_ADDRESS
       }};

static constexpr uint64_t START_HEIGHT = 2903240;
#else
static const bitcoin::core::Params CHAIN_PARAMS
    = {.BIP34_height = 21111,
       .BIP65_height = 581885,   // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
       .BIP66_height = 330776,   // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
       .CSV_height = 770112,     // 00000000025e930139bac5c6c31a403776da130831ab85be56578f3fa75369bb
       .Segwit_height = 834624,  // 00000000002b980fcd729daaa248fd9316a5200e9b367f4ff2c42453e84201ca
       .pow_limit = "0x00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
       .pow_target_timespan = 14 * 24 * 60 * 60,
       .pow_target_spacing = 10 * 60,
       .pow_allow_min_difficulty_blocks = false,
       .enforce_BIP94 = false,
       .bech32_hrp = "bc",
       .base58Prefixes = {
           std::vector<unsigned char>(1, 0),  // PUBKEY_ADDRESS
           std::vector<unsigned char>(1, 5),  // SCRIPT_ADDRESS
       }};
static constexpr uint64_t START_HEIGHT = 839999;
#endif

// CONTRACTS
static constexpr name BTC_CONTRACT = "btc.xsat"_n;
static constexpr name EXSAT_CONTRACT = "exsat.xsat"_n;
static constexpr name EVM_CONTRACT = "evm.xsat"_n;
static constexpr name ERC20_CONTRACT = "erc2o.xsat"_n;
static constexpr name POOL_REGISTER_CONTRACT = "poolreg.xsat"_n;
static constexpr name EVM_UTIL_CONTRACT = "evmutil.xsat"_n;
static constexpr name STAKING_CONTRACT = "staking.xsat"_n;
static constexpr name XSAT_STAKING_CONTRACT = "xsatstk.xsat"_n;
static constexpr name BLOCK_SYNC_CONTRACT = "blksync.xsat"_n;
static constexpr name BLOCK_ENDORSE_CONTRACT = "blkendt.xsat"_n;
static constexpr name ENDORSER_MANAGE_CONTRACT = "endrmng.xsat"_n;
static constexpr name UTXO_MANAGE_CONTRACT = "utxomng.xsat"_n;
static constexpr name RESOURCE_MANAGE_CONTRACT = "rescmng.xsat"_n;
static constexpr name REWARD_DISTRIBUTION_CONTRACT = "rwddist.xsat"_n;
static constexpr name CUSTODY_CONTRACT = "custody.xsat"_n;

// ACCOUNTS
static constexpr name VAULT_ACCOUNT = "vault.xsat"_n;
static constexpr name BOOT_ACCOUNT = "boot.xsat"_n;
static constexpr name TRIGGER_ACCOUNT = "trigger.xsat"_n;

// TABLE
static constexpr name BLOCK_CHUNK = "block.chunk"_n;

// SYMBOLS
static constexpr symbol XSAT_SYMBOL = {"XSAT", 8};
static constexpr symbol BTC_SYMBOL = {"BTC", 8};

// CONSTANTS
static constexpr uint64_t MAX_FUTURE_BLOCK_TIME = 2 * 60 * 60;
static constexpr checksum256 ZERO_HASH = checksum256();
static constexpr uint64_t GENESIS_ACTIVATION = 100000000LL;
static constexpr uint32_t DECIMAL = 100000000LL;
static constexpr uint64_t MIN_STAKE_FOR_ENDORSEMENT = 100LL * DECIMAL;
static constexpr uint64_t BTC_SUPPLY = 21000000LL * DECIMAL;
static constexpr uint64_t XSAT_REWARD_PER_BLOCK = 50LL * DECIMAL;
static constexpr uint64_t MIN_BTC_STAKE_FOR_VALIDATOR = 100LL * DECIMAL;
static constexpr uint64_t MIN_XSAT_STAKE_FOR_VALIDATOR = 21000LL * DECIMAL;
static constexpr uint64_t XSAT_SUPPLY = 21000000LL * DECIMAL;

static constexpr uint64_t IRREVERSIBLE_BLOCKS = 6;
static constexpr uint64_t SUBSIDY_HALVING_INTERVAL = 210000;

static constexpr uint64_t BLOCK_HEADER_SIZE = 80;
static constexpr uint64_t MAX_BLOCK_SIZE = 4LL * 1024 * 1024;
static constexpr uint8_t MAX_NUM_CHUNKS = 64;

static constexpr uint64_t DEFAULT_PRODUCTED_BLOCK_LIMIT = 432;
static constexpr uint64_t DEFAULT_NUM_SLOTS = 2;
static constexpr uint16_t MAX_NUM_SLOTS = 1000;

static constexpr uint8_t STAKE_RELEASE_CYCLE = 7;  // days

static constexpr uint16_t RATE_BASE_10000 = 10000;

static constexpr uint64_t RATE_BASE = 100000000LL;
static constexpr uint64_t SYNCHRONIZER_REWARD_RATE = RATE_BASE / 10;
static constexpr uint64_t MINER_REWARD_RATE = RATE_BASE / 2;
static constexpr uint64_t CONSENSUS_REWARD_RATE = RATE_BASE / 10;
static constexpr uint64_t MAX_UINT_24 = 16777215LL;  // 2^24 - 1

// ENUMS
typedef uint8_t fee_type;
static constexpr fee_type BUY_SLOT = 1;
static constexpr fee_type PUSH_CHUNK = 2;
static constexpr fee_type VERIFY = 3;
static constexpr fee_type ENDORSE = 4;
static constexpr fee_type PARSE = 5;
