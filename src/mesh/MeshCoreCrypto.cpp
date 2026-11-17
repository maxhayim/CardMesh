#include "MeshCoreCrypto.h"

#include <cstring>

#include "Aes.h"
#include "Sha256.h"

namespace cardmesh::mesh {

namespace {

constexpr std::size_t kChannelHashSize = 1;
constexpr std::size_t kMacSize = 2;
constexpr std::size_t kAesBlockSize = 16;
constexpr std::size_t kGroupTextHeaderSize = 5;  // timestamp(4 LE) + flags(1)
constexpr std::size_t kSecretBufferSize = 32;    // HMAC key: PSK zero-padded to 32 bytes

void secretToKeys(const std::uint8_t secret[16], AesEcb& aes, std::uint8_t hmacKey[kSecretBufferSize]) {
    aes.setKey(secret, 16);
    std::memset(hmacKey, 0, kSecretBufferSize);
    std::memcpy(hmacKey, secret, 16);
}

}  // namespace

std::uint8_t meshCoreChannelHash(const std::uint8_t secret[16]) {
    std::uint8_t digest[kSha256DigestSize];
    sha256(secret, 16, digest);
    return digest[0];
}

std::vector<std::uint8_t> encodeMeshCoreGroupText(const std::uint8_t secret[16], const std::string& senderName,
                                                   const std::string& text, std::uint32_t timestamp) {
    AesEcb aes;
    std::uint8_t hmacKey[kSecretBufferSize];
    secretToKeys(secret, aes, hmacKey);

    const std::string body = senderName + ": " + text;
    const std::size_t rawLen = kGroupTextHeaderSize + body.size();
    const std::size_t paddedLen = ((rawLen + kAesBlockSize - 1) / kAesBlockSize) * kAesBlockSize;

    std::vector<std::uint8_t> plain(paddedLen, 0);
    plain[0] = static_cast<std::uint8_t>(timestamp);
    plain[1] = static_cast<std::uint8_t>(timestamp >> 8);
    plain[2] = static_cast<std::uint8_t>(timestamp >> 16);
    plain[3] = static_cast<std::uint8_t>(timestamp >> 24);
    plain[4] = 0x00;  // TXT_TYPE_PLAIN, attempt 0
    std::memcpy(plain.data() + kGroupTextHeaderSize, body.data(), body.size());

    std::vector<std::uint8_t> ciphertext = plain;
    aes.encrypt(ciphertext.data(), ciphertext.size());

    std::uint8_t mac[kSha256DigestSize];
    hmacSha256(hmacKey, kSecretBufferSize, ciphertext.data(), ciphertext.size(), mac);

    std::vector<std::uint8_t> payload;
    payload.reserve(kChannelHashSize + kMacSize + ciphertext.size());
    payload.push_back(meshCoreChannelHash(secret));
    payload.push_back(mac[0]);
    payload.push_back(mac[1]);
    payload.insert(payload.end(), ciphertext.begin(), ciphertext.end());
    return payload;
}

bool decodeMeshCoreGroupText(const std::uint8_t secret[16], const std::uint8_t* payload, std::size_t payloadLen,
                              DecodedMeshCoreGroupText& out) {
    if (payloadLen < kChannelHashSize + kMacSize + kAesBlockSize) {
        return false;
    }
    if (payload[0] != meshCoreChannelHash(secret)) {
        return false;
    }

    const std::uint8_t* mac = payload + kChannelHashSize;
    const std::uint8_t* ciphertext = payload + kChannelHashSize + kMacSize;
    const std::size_t ciphertextLen = payloadLen - kChannelHashSize - kMacSize;
    if (ciphertextLen == 0 || ciphertextLen % kAesBlockSize != 0) {
        return false;
    }

    AesEcb aes;
    std::uint8_t hmacKey[kSecretBufferSize];
    secretToKeys(secret, aes, hmacKey);

    std::uint8_t expectedMac[kSha256DigestSize];
    hmacSha256(hmacKey, kSecretBufferSize, ciphertext, ciphertextLen, expectedMac);
    if (expectedMac[0] != mac[0] || expectedMac[1] != mac[1]) {
        return false;
    }

    std::vector<std::uint8_t> plain(ciphertext, ciphertext + ciphertextLen);
    aes.decrypt(plain.data(), plain.size());
    if (plain.size() < kGroupTextHeaderSize + 1) {
        return false;
    }

    // Group text is TXT_TYPE_PLAIN -- reject anything with the type bits set,
    // mirroring the firmware's onGroupDataRecv.
    if ((plain[kGroupTextHeaderSize - 1] >> 2) != 0) {
        return false;
    }

    out.timestamp = static_cast<std::uint32_t>(plain[0]) | (static_cast<std::uint32_t>(plain[1]) << 8) |
                    (static_cast<std::uint32_t>(plain[2]) << 16) | (static_cast<std::uint32_t>(plain[3]) << 24);

    std::size_t end = kGroupTextHeaderSize;
    while (end < plain.size() && plain[end] != 0) {
        ++end;
    }
    out.body.assign(reinterpret_cast<const char*>(plain.data()) + kGroupTextHeaderSize, end - kGroupTextHeaderSize);
    return true;
}

}  // namespace cardmesh::mesh
