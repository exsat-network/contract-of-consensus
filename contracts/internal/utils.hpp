#pragma once

#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>
#include <cstring>
#include <vector>
#include <string>
#include "defines.hpp"

using namespace eosio;
using namespace std;

namespace xsat::utils {

    static checksum256 compute_id(const checksum160& data) {
        array<uint8_t, 32U> output = {};
        auto input_bytes = data.extract_as_byte_array();
        copy(begin(input_bytes), end(input_bytes), begin(output) + 12);
        return checksum256(output);
    }

    static uint128_t compute_id(const extended_symbol& token) {
        return (uint128_t{token.get_contract().value} << 64) | token.get_symbol().code().raw();
    }

    static checksum256 compute_block_id(const uint64_t height, const checksum256& hash) {
        vector<char> result;
        result.resize(40);
        datastream<char*> ds(result.data(), result.size());
        ds << height;
        ds << hash;
        return sha256((char*)result.data(), result.size());
    }

    static checksum256 compute_utxo_id(const checksum256 &tx_id, const uint32_t index) {
        std::vector<char> result;
        result.resize(36);
        eosio::datastream<char *> ds(result.data(), result.size());
        ds << tx_id;
        ds << index;
        return eosio::sha256((char *)result.data(), result.size());
    }

    static checksum256 hash(const string& data) { return sha256(data.c_str(), data.size()); }

    static checksum160 hash_ripemd160(const string& data) { return ripemd160(data.c_str(), data.size()); }

    static checksum256 hash(const vector<uint8_t>& data) { return sha256((char*)data.data(), data.size()); }

    static name parse_name(const string& str) {
        if (str.length() == 0 || str.length() > 12) return {};
        int i = -1;
        for (const auto c : str) {
            i++;
            if (islower(c) || (isdigit(c) && c >= '1' && c <= '5') || c == '.') {
                if (i == 0 && c == '.') return {};
            } else
                return {};
        }
        return name{str};
    }

    static string account_decode(const vector<uint8_t>& name) {
        static const char* charmap = "abcdefghijklmnopqrstuvwxyz12345.";
        string result;
        for (const uint8_t c : name) {
            result.push_back(charmap[c]);
        }
        return result;
    }

    static vector<uint8_t> account_encode(const name& account) {
        static const char* charmap = "abcdefghijklmnopqrstuvwxyz12345.";
        const auto account_str = account.to_string();
        vector<uint8_t> result;
        result.reserve(account_str.size());
        for (const auto c : account_str) {
            uint8_t value = 0;
            if (c == '.')
                value = 32;
            else if (c >= '1' && c <= '5')
                value = (c - '1') + 26;
            else if (c >= 'a' && c <= 'z')
                value = c - 'a';
            result.push_back(value);
        }
        return result;
    }

    //  OP_RETURN: 6a
    //  DATA_LENGTH: 12
    //  XSAT: 4558534154
    //  VERSION: 01
    //  ACCOUNT: 1208060d04111f0417120013
    static name get_op_return_eos_account(const vector<uint8_t>& data) {
        if (data[0] != 0x6a) {
            return {};
        }
        if (data.size() - 2 != data[1]) return {};

        constexpr auto witness_commitment_header = array<uint8_t, 6>{0x45, 0x58, 0x53, 0x41, 0x54, 0x01};

        if (0 != memcmp(data.data() + 2, witness_commitment_header.data(), witness_commitment_header.size())) {
            return {};
        }
        vector<uint8_t> account_data(data.begin() + 8, data.end());
        return parse_name(account_decode(account_data));
    }

    static vector<uint8_t> encode_op_return_eos_account(const name& account) {
        auto length = account.length();
        vector<uint8_t> result = {0x6a, length, 0x45, 0x58, 0x53, 0x41, 0x54, 0x01};
        auto encoded_account = account_encode(account);
        copy(begin(encoded_account), end(encoded_account), end(result));
        return result;
    }

    // OP_FALSE OP_RETURN or OP_RETURN
    static bool is_unspendable_legacy(const vector<uint8_t>& script) {
        return (script[0] == 0x00 && script[1] == 0x6a) || script[0] == 0x6a;
    }

    // OP_FALSE OP_RETURN
    static bool is_unspendable_genesis(const vector<uint8_t>& script) { return script[0] == 0x00 && script[1] == 0x6a; }

    static bool is_unspendable(const uint64_t height, const vector<uint8_t>& script) {
        return height > GENESIS_ACTIVATION ? is_unspendable_genesis(script) : is_unspendable_legacy(script);
    }

    template <typename T>
    static bool is_valid_evm_address(const T& address) {
        bool hasPrefix = address.size() >= 2 && address[0] == '0' && address[1] == 'x';
        size_t expectedLength = hasPrefix ? 42 : 40;

        if (address.size() != expectedLength) {
            return false;
        }

        for (size_t i = hasPrefix ? 2 : 0; i < address.size(); ++i) {
            char c = address[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                return false;
            }
        }

        return true;
    }

    static vector<string> split(const string str, const string delim) {
        vector<string> tokens;
        if (str.size() == 0) return tokens;

        size_t prev = 0, pos = 0;
        do {
            pos = str.find(delim, prev);
            if (pos == string::npos) pos = str.length();
            string token = str.substr(prev, pos - prev);
            if (token.length() > 0) tokens.push_back(token);
            prev = pos + delim.length();
        } while (pos < str.length() && prev < str.length());
        return tokens;
    }

    static bool is_digit(const string str) {
        if (!str.size()) return false;
        for (const auto c : str) {
            if (!isdigit(c)) return false;
        }
        return true;
    }

    static symbol_code parse_symbol_code(const string& str) {
        if (str.size() > 7) return {};

        for (const auto c : str) {
            if (c < 'A' || c > 'Z') return {};
        }
        const symbol_code sym_code = symbol_code{str};

        return sym_code.is_valid() ? sym_code : symbol_code{};
    }

    static asset parse_asset(const string& str) {
        auto tokens = utils::split(str, " ");
        if (tokens.size() != 2) return {};

        auto sym_code = parse_symbol_code(tokens[1]);
        if (!sym_code.is_valid()) return {};

        int64_t amount = 0;
        uint8_t precision = 0, digits = 0;
        bool seen_dot = false, neg = false;
        for (const auto c : tokens[0]) {
            if (c == '-') {
                if (digits > 0) return {};
                neg = true;
                continue;
            }
            if (c == '.') {
                if (seen_dot || digits == 0) return {};
                seen_dot = true;
                continue;
            }
            if (!isdigit(c)) return {};
            digits++;
            if (seen_dot) precision++;
            amount = amount * 10 + (c - '0');
        }
        if (precision > 16 || (seen_dot && precision == 0)) return {};

        asset out = asset{neg ? -amount : amount, symbol{sym_code, precision}};
        if (!out.is_valid()) return {};

        return out;
    }

    static checksum256 get_trx_id() {
        auto size = transaction_size();
        char buf[size];
        uint32_t read = read_transaction(buf, size);
        check(size == read, "read_transaction failed");
        return sha256(buf, read);
    }

    static string to_hex(const char* d, uint32_t s) {
        string r;
        const char* to_hex = "0123456789abcdef";
        uint8_t* c = (uint8_t*)d;
        for (uint32_t i = 0; i < s; ++i) (r += to_hex[(c[i] >> 4)]) += to_hex[(c[i] & 0x0f)];
        return r;
    }

    static uint32_t num_reached_consensus(const uint32_t nums) { return nums * 2 / 3 + 1; }

    static string sha256_to_hex(const checksum256& sha256) {
        auto buffer = sha256.extract_as_byte_array();
        return to_hex((char*)buffer.data(), buffer.size());
    }

    static string sha1_to_hex(const checksum160& sha1) {
        auto buffer = sha1.extract_as_byte_array();
        return to_hex((char*)buffer.data(), buffer.size());
    }

    static vector<unsigned char> hex_to_bytes(const string& hex) {
        vector<unsigned char> result;
        unsigned char byte;
        for (size_t i = 0; i < hex.length(); i += 2) {
            sscanf(hex.c_str() + i, "%2hhx", &byte);
            result.push_back(byte);
        }
        return result;
    }

    static string bytes_to_hex(const std::vector<unsigned char>& data) {
        std::string result;
        const char* hex_chars = "0123456789abcdef";
        for (auto byte : data) {
            result.push_back(hex_chars[(byte >> 4) & 0x0F]);
            result.push_back(hex_chars[byte & 0x0F]);
        }
        return result;
    }

    static bool is_proxy_valid(const checksum160& proxy) {
        const uint64_t* data = reinterpret_cast<const uint64_t*>(&proxy);
        uint64_t first_part = data[0];
        uint64_t second_part = data[1];

        return first_part == 0 && (second_part & 0xFFFFFFFF00000000) == 0;
    }

    string checksum160_to_string(const checksum160& cs) {
        auto arr = cs.extract_as_byte_array();
        string result;
        for (auto byte : arr) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", byte);
            result += buf;
        }
        return result;
    }

    static uint8_t from_hex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        eosio::check(false, "Invalid hex character");
        return 0;
    }

    checksum160 evm_address_to_checksum160(const string& evm_address) {
        std::string address = evm_address.substr(0, 2) == "0x" ? evm_address.substr(2) : evm_address;
        eosio::check(address.length() == 40, "Invalid EVM address length");
        std::array<uint8_t, 20> bytes;
        for (int i = 0; i < 20; i++) {
            bytes[i] = (from_hex(address[i*2]) << 4) | from_hex(address[i*2+1]);
        }
        return checksum160(bytes);
    }

}  // namespace xsat::utils