#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cardmesh::mesh {

// MeshCore's well-known "Public" channel secret (firmware PUBLIC_GROUP_PSK,
// base64 "izOH6cXN6mrJ5e26oRXNcg=="), confirmed directly in
// meshcore-dev/MeshCore's examples/companion_radio/MyMesh.cpp and used
// identically by Yeraze/meshmonitor (the upstream MeshMonitor project).
constexpr std::uint8_t kMeshCorePublicChannelSecret[16] = {0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
                                                            0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72};

// A named MeshCore channel CardMesh can send/decode on -- mirrors
// cardmesh::mesh::KnownChannel's role for Meshtastic (see MeshPacketCodec.h).
// Only "Public" (kMeshCorePublicChannelSecret) is known by default; there's
// no MeshMonitor-equivalent server to pull additional MeshCore channel
// keys from, so this is mainly infrastructure for whenever a real second
// channel key is configured.
struct KnownMeshCoreChannel {
    std::string name;
    std::uint8_t secret[16];
};

// 1-byte channel hash = SHA-256(secret)[0] (BaseChatMesh::addChannel).
std::uint8_t meshCoreChannelHash(const std::uint8_t secret[16]);

// Builds a GRP_TXT payload: [channel_hash:1][MAC:2][ciphertext], matching
// meshcore-dev/MeshCore's Mesh::createGroupDatagram + Utils::encryptThenMAC
// exactly (verified against Yeraze/meshmonitor's independently-implemented,
// production-tested reference in src/server/utils/meshcoreGroupEcho.ts).
// Plaintext is timestamp(4 LE) + flags(1, 0x00 for plain text) +
// "<senderName>: <text>", zero-padded to a 16-byte boundary (not PKCS7).
std::vector<std::uint8_t> encodeMeshCoreGroupText(const std::uint8_t secret[16], const std::string& senderName,
                                                   const std::string& text, std::uint32_t timestamp);

struct DecodedMeshCoreGroupText {
    std::uint32_t timestamp = 0;
    std::string body;  // "<senderName>: <text>" -- not yet split apart
};

// Inverse of encodeMeshCoreGroupText: MAC-verifies then decrypts. Returns
// false on a wrong channel hash, MAC mismatch, malformed structure, or a
// non-plain-text group message -- mirroring tryDecodeGroupTextPayload's
// "never throws, just rejects" behavior.
bool decodeMeshCoreGroupText(const std::uint8_t secret[16], const std::uint8_t* payload, std::size_t payloadLen,
                              DecodedMeshCoreGroupText& out);

}  // namespace cardmesh::mesh
