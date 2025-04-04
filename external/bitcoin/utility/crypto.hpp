#pragma once

#include <bitcoin/utility/types.hpp>
#include <eosio/crypto.hpp>
#include <eosio/datastream.hpp>
#include <eosio/serialize.hpp>

namespace bitcoin {
    template <typename T>
    bitcoin::uint256_t dhash(const T& data) {
        auto packed_data = eosio::pack(data);
        auto h = eosio::sha256((char*)packed_data.data(), packed_data.size());
        auto buffer = h.extract_as_byte_array();
        h = eosio::sha256((char*)buffer.data(), buffer.size());
        return bitcoin::le_uint_from_checksum256(h);
    }

    template <typename T>
    bitcoin::uint256_t dhash(T* data, size_t len) {
        auto h = eosio::sha256(data, len);
        auto buffer = h.extract_as_byte_array();
        h = eosio::sha256((char*)buffer.data(), buffer.size());
        return bitcoin::le_uint_from_checksum256(h);
    }

    template <typename T>
    auto dhash(const std::vector<T>& data) -> std::enable_if_t<sizeof(T) == 1, bitcoin::uint256_t> {
        auto h = eosio::sha256((char*)data.data(), data.size());
        auto buffer = h.extract_as_byte_array();
        h = eosio::sha256((char*)buffer.data(), buffer.size());
        return bitcoin::le_uint_from_checksum256(h);
    }

    template <typename T, size_t N>
    auto dhash(const std::array<T, N>& data) -> std::enable_if_t<sizeof(T) == 1, bitcoin::uint256_t> {
        auto h = eosio::sha256((char*)data.data(), data.size());
        auto buffer = h.extract_as_byte_array();
        h = eosio::sha256((char*)buffer.data(), buffer.size());
        return bitcoin::le_uint_from_checksum256(h);
    }

    bitcoin::uint256_t generate_merkle_root(std::vector<bitcoin::uint256_t>& hashes, bool* mutated = nullptr) {
        bool mutation = false;
        while (hashes.size() > 1) {
            if (mutated) {
                for (size_t pos = 0; pos + 1 < hashes.size(); pos += 2) {
                    if (hashes[pos] == hashes[pos + 1]) mutation = true;
                }
            }

            if (hashes.size() % 2 == 1) {
                // odd number of hashes duplicate the last hash
                hashes.push_back(hashes.back());
            }

            // combine hashes
            auto new_hashes = std::vector<bitcoin::uint256_t>();
            new_hashes.reserve(hashes.size() / 2);
            for (auto i = 0; i < hashes.size(); i += 2) {
                auto concatenated_hashes = std::array<uint8_t, 64>();
                auto ds = eosio::datastream<uint8_t*>(concatenated_hashes.data(), concatenated_hashes.size());
                ds << hashes[i] << hashes[i + 1];

                auto h = bitcoin::dhash(concatenated_hashes);
                new_hashes.push_back(h);
            }

            hashes = std::move(new_hashes);
        }
        if (mutated) *mutated = mutation;
        if (hashes.size() == 0) return bitcoin::uint256_t(0);
        return hashes[0];
    }

    bitcoin::uint256_t generate_merkle_root(std::vector<eosio::checksum256>& hashes, bool* mutated = nullptr) {
        std::vector<bitcoin::uint256_t> data;
        data.reserve(hashes.size());
        for (const auto& hash : hashes) {
            auto h = bitcoin::le_uint_from_checksum256(hash);
            data.emplace_back(std::move(h));
        }
        return generate_merkle_root(data, mutated);
    }

}  // namespace bitcoin