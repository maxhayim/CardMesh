#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Protobuf.h"

namespace cardmesh::mesh {

// Meshtastic's 16-byte on-air header (RadioInterface.h PacketHeader),
// verbatim field-for-field -- "has to exactly match the wire layout".
struct PacketHeader {
    std::uint32_t to = 0;
    std::uint32_t from = 0;
    std::uint32_t id = 0;
    std::uint8_t flags = 0;
    std::uint8_t channelHash = 0;
    std::uint8_t nextHop = 0;
    std::uint8_t relayNode = 0;

    static constexpr std::uint32_t kBroadcast = 0xFFFFFFFF;

    std::uint8_t hopLimit() const { return flags & 0x07; }
    bool wantAck() const { return (flags & 0x08) != 0; }
    bool viaMqtt() const { return (flags & 0x10) != 0; }
    std::uint8_t hopStart() const { return static_cast<std::uint8_t>((flags & 0xE0) >> 5); }

    void setHopLimit(std::uint8_t hopLimit) { flags = static_cast<std::uint8_t>((flags & ~0x07) | (hopLimit & 0x07)); }
    void setHopStart(std::uint8_t hopStart) {
        flags = static_cast<std::uint8_t>((flags & ~0xE0) | ((hopStart & 0x07) << 5));
    }
};

// A channel this node knows about, for decrypting/identifying inbound
// packets and for encrypting outbound ones. `key` is the resolved raw key
// bytes (16 or 32), not a PSK index -- callers resolve `expandDefaultPsk`
// or decode the channel's base64 PSK before constructing this.
struct KnownChannel {
    std::string name;  // resolved display name (e.g. "LongFast" for an empty Primary channel name)
    std::vector<std::uint8_t> key;
};

// Builds a full on-air frame: 16-byte PacketHeader followed by the
// AES-CTR-encrypted Data protobuf. Ready to hand to LoraRadio::send().
std::vector<std::uint8_t> encodePacket(std::uint32_t fromNode, std::uint32_t toNode, std::uint32_t packetId,
                                       const KnownChannel& channel, std::uint32_t portnum,
                                       const std::uint8_t* payload, std::size_t payloadLength, std::uint8_t hopLimit = 3);

struct DecodedPacket {
    PacketHeader header;
    pb::DataMessage data;
    std::string channelName;  // which KnownChannel matched, by hash
};

// Parses a received on-air frame against the given candidate channels
// (matched by the header's channel-hash byte, same as Meshtastic's own
// receiver-side channel guessing) and decrypts/decodes it on a match.
bool decodePacket(const std::uint8_t* frame, std::size_t length, const std::vector<KnownChannel>& candidates,
                   DecodedPacket& out);

}  // namespace cardmesh::mesh
