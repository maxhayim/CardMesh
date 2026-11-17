#pragma once

#include <cstdint>
#include <vector>

namespace cardmesh::mesh {

// MeshCore wire packet framing (meshcore-dev/MeshCore's Packet::writeTo /
// Packet::readFrom): [header:1][transport_codes:4, if route has them]
// [path_len:1][path:path_len bytes][payload:remaining bytes].
// CardMesh only ever sends ROUTE_TYPE_FLOOD with an empty path (path_len=0),
// the simplest valid encoding -- repeaters build up the path as they flood
// it. RX must still parse any route type correctly to read overheard
// packets from other nodes.

constexpr std::uint8_t kMeshCoreRouteTypeTransportFlood = 0x00;
constexpr std::uint8_t kMeshCoreRouteTypeFlood = 0x01;
constexpr std::uint8_t kMeshCoreRouteTypeDirect = 0x02;
constexpr std::uint8_t kMeshCoreRouteTypeTransportDirect = 0x03;

constexpr std::uint8_t kMeshCorePayloadTypeGrpTxt = 0x05;

constexpr std::size_t kMeshCoreMaxPathSize = 64;

struct MeshCoreRawPacket {
    std::uint8_t header = 0;
    std::uint16_t transportCodes[2] = {0, 0};
    std::vector<std::uint8_t> path;  // raw path-hash bytes, opaque to us
    std::vector<std::uint8_t> payload;

    std::uint8_t routeType() const { return header & 0x03; }
    std::uint8_t payloadType() const { return (header >> 2) & 0x0F; }
    std::uint8_t payloadVer() const { return (header >> 6) & 0x03; }
    bool hasTransportCodes() const {
        return routeType() == kMeshCoreRouteTypeTransportFlood || routeType() == kMeshCoreRouteTypeTransportDirect;
    }
};

std::uint8_t makeMeshCoreHeader(std::uint8_t routeType, std::uint8_t payloadType, std::uint8_t payloadVer = 0);

// Builds the wire bytes for a flood packet with no prior path (our own
// originated sends always start this way).
std::vector<std::uint8_t> encodeMeshCoreFloodPacket(std::uint8_t payloadType, const std::vector<std::uint8_t>& payload);

// Parses any overheard packet. Returns false on a malformed/truncated frame
// -- never throws, since this runs on the hot inbound-packet path.
bool decodeMeshCoreRawPacket(const std::uint8_t* data, std::size_t length, MeshCoreRawPacket& out);

}  // namespace cardmesh::mesh
