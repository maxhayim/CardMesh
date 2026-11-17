#include "MeshCorePacketCodec.h"

namespace cardmesh::mesh {

std::uint8_t makeMeshCoreHeader(std::uint8_t routeType, std::uint8_t payloadType, std::uint8_t payloadVer) {
    return static_cast<std::uint8_t>((routeType & 0x03) | ((payloadType & 0x0F) << 2) | ((payloadVer & 0x03) << 6));
}

std::vector<std::uint8_t> encodeMeshCoreFloodPacket(std::uint8_t payloadType, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    out.reserve(2 + payload.size());
    out.push_back(makeMeshCoreHeader(kMeshCoreRouteTypeFlood, payloadType));
    out.push_back(0);  // path_len = 0: no path yet, repeaters build it up
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool decodeMeshCoreRawPacket(const std::uint8_t* data, std::size_t length, MeshCoreRawPacket& out) {
    std::size_t i = 0;
    if (length < 1) {
        return false;
    }
    out.header = data[i++];

    if (out.hasTransportCodes()) {
        if (length < i + 4) {
            return false;
        }
        out.transportCodes[0] = static_cast<std::uint16_t>(data[i] | (data[i + 1] << 8));
        i += 2;
        out.transportCodes[1] = static_cast<std::uint16_t>(data[i] | (data[i + 1] << 8));
        i += 2;
    } else {
        out.transportCodes[0] = out.transportCodes[1] = 0;
    }

    if (length < i + 1) {
        return false;
    }
    const std::uint8_t pathLenByte = data[i++];
    const std::uint8_t hashSize = static_cast<std::uint8_t>((pathLenByte >> 6) + 1);
    const std::uint8_t hashCount = static_cast<std::uint8_t>(pathLenByte & 63);
    if (hashSize == 4) {
        return false;  // reserved
    }
    const std::size_t pathBytes = static_cast<std::size_t>(hashCount) * hashSize;
    if (pathBytes > kMeshCoreMaxPathSize || length < i + pathBytes) {
        return false;
    }
    out.path.assign(data + i, data + i + pathBytes);
    i += pathBytes;

    if (i >= length) {
        return false;  // no payload: bad encoding (mirrors firmware's readFrom)
    }
    out.payload.assign(data + i, data + length);
    return true;
}

}  // namespace cardmesh::mesh
