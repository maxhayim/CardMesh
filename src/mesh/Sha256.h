#pragma once

#include <cstddef>
#include <cstdint>

namespace cardmesh::mesh {

constexpr std::size_t kSha256DigestSize = 32;
constexpr std::size_t kSha256BlockSize = 64;

// Standard FIPS-180-4 SHA-256, implemented from scratch (no external crypto
// library in this project) and verified against the official NIST test
// vectors (empty string, "abc") -- see scratchpad test, not committed.
void sha256(const std::uint8_t* data, std::size_t length, std::uint8_t digest[kSha256DigestSize]);

// RFC 2104 HMAC-SHA256.
void hmacSha256(const std::uint8_t* key, std::size_t keyLength, const std::uint8_t* data, std::size_t dataLength,
                 std::uint8_t mac[kSha256DigestSize]);

}  // namespace cardmesh::mesh
