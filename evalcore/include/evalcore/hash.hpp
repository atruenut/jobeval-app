// Hashes and encodings used to verify downloaded packs.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jobeval {

using Bytes = std::vector<uint8_t>;

std::array<uint8_t, 32> sha256(const uint8_t* data, size_t size);
std::array<uint8_t, 64> sha512(const uint8_t* data, size_t size);
std::array<uint8_t, 64> blake2b512(const uint8_t* data, size_t size);  // Monocypher

inline std::array<uint8_t, 32> sha256(const std::string& s) {
    return sha256(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

std::string to_hex(const uint8_t* data, size_t size);  // lowercase
std::string sha256_hex(const std::string& data);

bool base64_decode(const std::string& text, Bytes& out);  // standard alphabet, padding required
std::string base64_encode(const uint8_t* data, size_t size);

}  // namespace jobeval
