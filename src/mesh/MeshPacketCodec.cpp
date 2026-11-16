#include "MeshPacketCodec.h"

#include <cstring>

#include "MeshtasticCrypto.h"

namespace cardmesh::mesh {

namespace {

constexpr std::size_t kHeaderLength = 16;

void writeHeader(const PacketHeader& header, std::uint8_t* out) {
    std::memcpy(out + 0, &header.to, 4);
    std::memcpy(out + 4, &header.from, 4);
    std::memcpy(out + 8, &header.id, 4);
    out[12] = header.flags;
    out[13] = header.channelHash;
    out[14] = header.nextHop;
    out[15] = header.relayNode;
}

void readHeader(const std::uint8_t* in, PacketHeader& header) {
    std::memcpy(&header.to, in + 0, 4);
    std::memcpy(&header.from, in + 4, 4);
    std::memcpy(&header.id, in + 8, 4);
    header.flags = in[12];
    header.channelHash = in[13];
    header.nextHop = in[14];
    header.relayNode = in[15];
}

}  // namespace

std::vector<std::uint8_t> encodePacket(std::uint32_t fromNode, std::uint32_t toNode, std::uint32_t packetId,
                                       const KnownChannel& channel, std::uint32_t portnum,
                                       const std::uint8_t* payload, std::size_t payloadLength, std::uint8_t hopLimit) {
    pb::DataMessage data;
    data.portnum = portnum;
    data.payload.assign(payload, payload + payloadLength);
    std::vector<std::uint8_t> plainData = pb::encodeData(data);

    channelCrypt(channel.key.data(), channel.key.size(), packetId, fromNode, plainData.data(), plainData.size());

    PacketHeader header;
    header.to = toNode;
    header.from = fromNode;
    header.id = packetId;
    header.setHopLimit(hopLimit);
    header.setHopStart(hopLimit);
    header.channelHash = channelHash(channel.name.c_str(), channel.key.data(), channel.key.size());
    header.nextHop = 0;
    header.relayNode = 0;

    std::vector<std::uint8_t> out(kHeaderLength + plainData.size());
    writeHeader(header, out.data());
    std::memcpy(out.data() + kHeaderLength, plainData.data(), plainData.size());
    return out;
}

bool decodePacket(const std::uint8_t* frame, std::size_t length, const std::vector<KnownChannel>& candidates,
                   DecodedPacket& out) {
    if (length < kHeaderLength) {
        return false;
    }
    readHeader(frame, out.header);

    const std::uint8_t* encrypted = frame + kHeaderLength;
    const std::size_t encryptedLength = length - kHeaderLength;

    for (const auto& channel : candidates) {
        const std::uint8_t hash = channelHash(channel.name.c_str(), channel.key.data(), channel.key.size());
        if (hash != out.header.channelHash) {
            continue;
        }

        std::vector<std::uint8_t> plain(encrypted, encrypted + encryptedLength);
        channelCrypt(channel.key.data(), channel.key.size(), out.header.id, out.header.from, plain.data(), plain.size());

        pb::DataMessage data;
        if (!pb::decodeData(plain.data(), plain.size(), data)) {
            continue;  // hash collision against a different channel; keep trying
        }
        out.data = data;
        out.channelName = channel.name;
        return true;
    }
    return false;
}

}  // namespace cardmesh::mesh
