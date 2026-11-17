#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cardmesh::mesh {

// Forward-direction-only AES-128/256 block cipher plus CTR-mode keystream
// generation, matching Meshtastic firmware's CryptoEngine::encryptAESCtr:
// a 16-byte IV/nonce with the last 4 bytes used as a big-endian block
// counter. CTR mode only ever runs the cipher in the encrypt direction (for
// both encrypting and decrypting), so no inverse cipher is implemented.
class AesCtr {
public:
    // keyBytes must be 16 (AES-128) or 32 (AES-256).
    bool setKey(const std::uint8_t* keyBytes, std::size_t keyLength);

    // XORs `data` in place with the AES-CTR keystream derived from `nonce`
    // (16 bytes, not modified) and this object's key.
    void process(const std::uint8_t nonce[16], std::uint8_t* data, std::size_t length) const;

private:
    std::vector<std::uint8_t> roundKeys_;
    int rounds_ = 0;

    void encryptBlock(const std::uint8_t in[16], std::uint8_t out[16]) const;
};

// AES-128 ECB, forward and inverse, for MeshCore's GroupChannel crypto
// (Utils::encrypt/decrypt in meshcore-dev/MeshCore): each 16-byte block
// is independently AES-encrypted/decrypted with no chaining. The caller
// is responsible for zero-padding plaintext to a block boundary --
// MeshCore does not use PKCS7 padding.
class AesEcb {
public:
    // keyBytes must be 16 (AES-128 only -- MeshCore's companion protocol
    // doesn't support 256-bit channel keys).
    bool setKey(const std::uint8_t* keyBytes, std::size_t keyLength);

    // length must be a multiple of 16; encrypts/decrypts in place block by
    // block (ECB has no chaining, so in-place is safe).
    void encrypt(std::uint8_t* data, std::size_t length) const;
    void decrypt(std::uint8_t* data, std::size_t length) const;

private:
    std::vector<std::uint8_t> roundKeys_;
    int rounds_ = 10;  // AES-128 only

    void encryptBlock(const std::uint8_t in[16], std::uint8_t out[16]) const;
    void decryptBlock(const std::uint8_t in[16], std::uint8_t out[16]) const;
};

}  // namespace cardmesh::mesh
