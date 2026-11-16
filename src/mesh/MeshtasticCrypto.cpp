#include "MeshtasticCrypto.h"

#include <cstring>

#include "Aes.h"

namespace cardmesh::mesh {

namespace {

// meshtastic/firmware src/mesh/Channels.h: static const uint8_t defaultpsk[].
constexpr std::uint8_t kDefaultPsk[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
                                          0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};

std::uint8_t xorHash(const std::uint8_t* p, std::size_t len) {
    std::uint8_t code = 0;
    for (std::size_t i = 0; i < len; ++i) {
        code ^= p[i];
    }
    return code;
}

// meshtastic/firmware src/mesh/RadioInterface.cpp: uint32_t hash(const char*). djb2.
std::uint32_t djb2(const char* str) {
    std::uint32_t hash = 5381;
    int c;
    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    return hash;
}

}  // namespace

std::vector<std::uint8_t> expandDefaultPsk(std::uint8_t index) {
    std::vector<std::uint8_t> key(kDefaultPsk, kDefaultPsk + sizeof(kDefaultPsk));
    key.back() = static_cast<std::uint8_t>(key.back() + index - 1);
    return key;
}

void buildNonce(std::uint32_t packetId, std::uint32_t fromNode, std::uint8_t nonce[16]) {
    std::memset(nonce, 0, 16);
    nonce[0] = static_cast<std::uint8_t>(packetId);
    nonce[1] = static_cast<std::uint8_t>(packetId >> 8);
    nonce[2] = static_cast<std::uint8_t>(packetId >> 16);
    nonce[3] = static_cast<std::uint8_t>(packetId >> 24);
    // bytes 4-7 stay zero: packetId is only 32 bits, zero-extended to the
    // 64-bit field CryptoEngine::initNonce uses.
    nonce[8] = static_cast<std::uint8_t>(fromNode);
    nonce[9] = static_cast<std::uint8_t>(fromNode >> 8);
    nonce[10] = static_cast<std::uint8_t>(fromNode >> 16);
    nonce[11] = static_cast<std::uint8_t>(fromNode >> 24);
    // bytes 12-15 (extraNonce) stay zero for normal channel-PSK traffic.
}

bool channelCrypt(const std::uint8_t* key, std::size_t keyLength, std::uint32_t packetId, std::uint32_t fromNode,
                   std::uint8_t* data, std::size_t length) {
    AesCtr cipher;
    if (!cipher.setKey(key, keyLength)) {
        return false;
    }
    std::uint8_t nonce[16];
    buildNonce(packetId, fromNode, nonce);
    cipher.process(nonce, data, length);
    return true;
}

std::uint8_t channelHash(const char* name, const std::uint8_t* key, std::size_t keyLength) {
    std::uint8_t h = xorHash(reinterpret_cast<const std::uint8_t*>(name), std::strlen(name));
    h ^= xorHash(key, keyLength);
    return h;
}

float us915MediumFastPrimaryFrequencyMhz() {
    // RDEF(US, 902.0f, 928.0f, ...) with PROFILE_STD (spacing=0, padding=0)
    // and MEDIUM_FAST's bw=250kHz (same as LongFast): freqSlotWidth = 0.25
    // MHz, numFreqSlots = round((928-902+0)/0.25) = 104. Only the hashed
    // name differs from LongFast, landing on a different slot.
    constexpr float kFreqStart = 902.0f;
    constexpr float kBwMhz = 0.25f;
    constexpr std::uint32_t kNumFreqSlots = 104;
    const std::uint32_t slot = djb2("MediumFast") % kNumFreqSlots;
    return kFreqStart + (kBwMhz / 2.0f) + static_cast<float>(slot) * kBwMhz;
}

}  // namespace cardmesh::mesh
