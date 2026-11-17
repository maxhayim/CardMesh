#include "Sha256.h"

#include <cstring>
#include <vector>

namespace cardmesh::mesh {

namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
    0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
    0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2,
};

std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

struct Sha256State {
    std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    void processBlock(const std::uint8_t block[64]) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) | static_cast<std::uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = hh + s1 + ch + kK[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }
};

}  // namespace

void sha256(const std::uint8_t* data, std::size_t length, std::uint8_t digest[kSha256DigestSize]) {
    Sha256State state;

    std::size_t offset = 0;
    while (offset + 64 <= length) {
        state.processBlock(data + offset);
        offset += 64;
    }

    // Final padded block(s): remaining data, 0x80, zero padding, 8-byte
    // big-endian bit length -- may spill into a second block.
    std::vector<std::uint8_t> tail(data + offset, data + length);
    const std::uint64_t bitLength = static_cast<std::uint64_t>(length) * 8;
    tail.push_back(0x80);
    while (tail.size() % 64 != 56) {
        tail.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        tail.push_back(static_cast<std::uint8_t>(bitLength >> (i * 8)));
    }
    for (std::size_t i = 0; i < tail.size(); i += 64) {
        state.processBlock(tail.data() + i);
    }

    for (int i = 0; i < 8; ++i) {
        digest[i * 4] = static_cast<std::uint8_t>(state.h[i] >> 24);
        digest[i * 4 + 1] = static_cast<std::uint8_t>(state.h[i] >> 16);
        digest[i * 4 + 2] = static_cast<std::uint8_t>(state.h[i] >> 8);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(state.h[i]);
    }
}

void hmacSha256(const std::uint8_t* key, std::size_t keyLength, const std::uint8_t* data, std::size_t dataLength,
                 std::uint8_t mac[kSha256DigestSize]) {
    std::uint8_t keyBlock[kSha256BlockSize] = {0};
    if (keyLength > kSha256BlockSize) {
        sha256(key, keyLength, keyBlock);  // keys longer than the block size are hashed down first
    } else {
        std::memcpy(keyBlock, key, keyLength);
    }

    std::uint8_t ipad[kSha256BlockSize];
    std::uint8_t opad[kSha256BlockSize];
    for (std::size_t i = 0; i < kSha256BlockSize; ++i) {
        ipad[i] = static_cast<std::uint8_t>(keyBlock[i] ^ 0x36);
        opad[i] = static_cast<std::uint8_t>(keyBlock[i] ^ 0x5c);
    }

    std::vector<std::uint8_t> inner(kSha256BlockSize + dataLength);
    std::memcpy(inner.data(), ipad, kSha256BlockSize);
    std::memcpy(inner.data() + kSha256BlockSize, data, dataLength);
    std::uint8_t innerDigest[kSha256DigestSize];
    sha256(inner.data(), inner.size(), innerDigest);

    std::uint8_t outer[kSha256BlockSize + kSha256DigestSize];
    std::memcpy(outer, opad, kSha256BlockSize);
    std::memcpy(outer + kSha256BlockSize, innerDigest, kSha256DigestSize);
    sha256(outer, sizeof(outer), mac);
}

}  // namespace cardmesh::mesh
