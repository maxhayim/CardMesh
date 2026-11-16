#include "Aes.h"

#include <cstring>
#include <utility>

namespace cardmesh::mesh {

namespace {

constexpr std::uint8_t kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9,
    0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f,
    0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07,
    0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3,
    0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58,
    0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3,
    0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f,
    0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac,
    0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a,
    0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70,
    0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42,
    0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

constexpr std::uint8_t kRcon[15] = {0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d};

std::uint8_t xtime(std::uint8_t x) { return static_cast<std::uint8_t>((x << 1) ^ ((x >> 7) * 0x1b)); }

// KeyExpansion into `Nb*(Nr+1)` words (4 bytes each), per FIPS-197.
void keyExpansion(const std::uint8_t* key, int nk, int rounds, std::uint8_t* roundKeys) {
    const int totalWords = 4 * (rounds + 1);
    std::memcpy(roundKeys, key, static_cast<std::size_t>(nk) * 4);

    std::uint8_t tmp[4];
    for (int i = nk; i < totalWords; ++i) {
        std::memcpy(tmp, roundKeys + (i - 1) * 4, 4);
        if (i % nk == 0) {
            const std::uint8_t t0 = tmp[0];
            tmp[0] = static_cast<std::uint8_t>(kSbox[tmp[1]] ^ kRcon[i / nk]);
            tmp[1] = kSbox[tmp[2]];
            tmp[2] = kSbox[tmp[3]];
            tmp[3] = static_cast<std::uint8_t>(kSbox[t0]);
        } else if (nk > 6 && i % nk == 4) {
            for (auto& b : tmp) {
                b = kSbox[b];
            }
        }
        for (int j = 0; j < 4; ++j) {
            roundKeys[i * 4 + j] = static_cast<std::uint8_t>(roundKeys[(i - nk) * 4 + j] ^ tmp[j]);
        }
    }
}

void addRoundKey(int round, std::uint8_t state[4][4], const std::uint8_t* roundKeys) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            state[row][col] = static_cast<std::uint8_t>(state[row][col] ^ roundKeys[round * 16 + col * 4 + row]);
        }
    }
}

void subBytes(std::uint8_t state[4][4]) {
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            state[row][col] = kSbox[state[row][col]];
        }
    }
}

void shiftRows(std::uint8_t state[4][4]) {
    std::uint8_t temp = state[1][0];
    state[1][0] = state[1][1];
    state[1][1] = state[1][2];
    state[1][2] = state[1][3];
    state[1][3] = temp;

    std::swap(state[2][0], state[2][2]);
    std::swap(state[2][1], state[2][3]);

    temp = state[3][3];
    state[3][3] = state[3][2];
    state[3][2] = state[3][1];
    state[3][1] = state[3][0];
    state[3][0] = temp;
}

void mixColumns(std::uint8_t state[4][4]) {
    for (int col = 0; col < 4; ++col) {
        const std::uint8_t a0 = state[0][col];
        const std::uint8_t a1 = state[1][col];
        const std::uint8_t a2 = state[2][col];
        const std::uint8_t a3 = state[3][col];
        const std::uint8_t t = static_cast<std::uint8_t>(a0 ^ a1 ^ a2 ^ a3);
        state[0][col] = static_cast<std::uint8_t>(a0 ^ t ^ xtime(static_cast<std::uint8_t>(a0 ^ a1)));
        state[1][col] = static_cast<std::uint8_t>(a1 ^ t ^ xtime(static_cast<std::uint8_t>(a1 ^ a2)));
        state[2][col] = static_cast<std::uint8_t>(a2 ^ t ^ xtime(static_cast<std::uint8_t>(a2 ^ a3)));
        state[3][col] = static_cast<std::uint8_t>(a3 ^ t ^ xtime(static_cast<std::uint8_t>(a3 ^ a0)));
    }
}

}  // namespace

bool AesCtr::setKey(const std::uint8_t* keyBytes, std::size_t keyLength) {
    int nk;
    if (keyLength == 16) {
        nk = 4;
        rounds_ = 10;
    } else if (keyLength == 32) {
        nk = 8;
        rounds_ = 14;
    } else {
        return false;
    }
    roundKeys_.assign(static_cast<std::size_t>(16 * (rounds_ + 1)), 0);
    keyExpansion(keyBytes, nk, rounds_, roundKeys_.data());
    return true;
}

void AesCtr::encryptBlock(const std::uint8_t in[16], std::uint8_t out[16]) const {
    std::uint8_t state[4][4];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            state[row][col] = in[col * 4 + row];
        }
    }

    addRoundKey(0, state, roundKeys_.data());
    for (int round = 1; round < rounds_; ++round) {
        subBytes(state);
        shiftRows(state);
        mixColumns(state);
        addRoundKey(round, state, roundKeys_.data());
    }
    subBytes(state);
    shiftRows(state);
    addRoundKey(rounds_, state, roundKeys_.data());

    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out[col * 4 + row] = state[row][col];
        }
    }
}

void AesCtr::process(const std::uint8_t nonce[16], std::uint8_t* data, std::size_t length) const {
    std::uint8_t counterBlock[16];
    std::memcpy(counterBlock, nonce, 16);
    // Matches the rweather/Crypto library's CTR<AES>::setCounterSize(4):
    // only the last 4 bytes of the 16-byte block act as a big-endian
    // counter that increments per block; the rest of the IV is fixed.
    std::uint32_t counter = (static_cast<std::uint32_t>(counterBlock[12]) << 24) |
                            (static_cast<std::uint32_t>(counterBlock[13]) << 16) |
                            (static_cast<std::uint32_t>(counterBlock[14]) << 8) | counterBlock[15];

    std::uint8_t keystream[16];
    std::size_t offset = 0;
    while (offset < length) {
        counterBlock[12] = static_cast<std::uint8_t>(counter >> 24);
        counterBlock[13] = static_cast<std::uint8_t>(counter >> 16);
        counterBlock[14] = static_cast<std::uint8_t>(counter >> 8);
        counterBlock[15] = static_cast<std::uint8_t>(counter);
        encryptBlock(counterBlock, keystream);
        ++counter;

        const std::size_t chunk = (length - offset) < 16 ? (length - offset) : 16;
        for (std::size_t i = 0; i < chunk; ++i) {
            data[offset + i] = static_cast<std::uint8_t>(data[offset + i] ^ keystream[i]);
        }
        offset += chunk;
    }
}

}  // namespace cardmesh::mesh
