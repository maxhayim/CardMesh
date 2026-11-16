#include "Protobuf.h"

#include <cstring>

namespace cardmesh::mesh::pb {

void appendVarint(std::vector<std::uint8_t>& out, std::uint64_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<std::uint8_t>(value) | 0x80);
        value >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(value));
}

void appendTag(std::vector<std::uint8_t>& out, int fieldNumber, int wireType) {
    appendVarint(out, static_cast<std::uint64_t>((fieldNumber << 3) | wireType));
}

void appendBytesField(std::vector<std::uint8_t>& out, int fieldNumber, const std::uint8_t* data, std::size_t length) {
    appendTag(out, fieldNumber, 2);
    appendVarint(out, length);
    out.insert(out.end(), data, data + length);
}

void appendVarintField(std::vector<std::uint8_t>& out, int fieldNumber, std::uint64_t value) {
    appendTag(out, fieldNumber, 0);
    appendVarint(out, value);
}

bool readVarint(const std::uint8_t* data, std::size_t length, std::size_t& offset, std::uint64_t& value) {
    value = 0;
    int shift = 0;
    while (true) {
        if (offset >= length || shift > 63) {
            return false;
        }
        const std::uint8_t byte = data[offset++];
        value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }
}

std::vector<std::uint8_t> encodeData(const DataMessage& msg) {
    std::vector<std::uint8_t> out;
    appendVarintField(out, 1, msg.portnum);
    if (!msg.payload.empty()) {
        appendBytesField(out, 2, msg.payload.data(), msg.payload.size());
    }
    return out;
}

bool decodeData(const std::uint8_t* data, std::size_t length, DataMessage& out) {
    std::size_t offset = 0;
    while (offset < length) {
        std::uint64_t tag;
        if (!readVarint(data, length, offset, tag)) {
            return false;
        }
        const int fieldNumber = static_cast<int>(tag >> 3);
        const int wireType = static_cast<int>(tag & 0x7);

        switch (wireType) {
            case 0: {  // varint
                std::uint64_t value;
                if (!readVarint(data, length, offset, value)) {
                    return false;
                }
                if (fieldNumber == 1) {
                    out.portnum = static_cast<std::uint32_t>(value);
                }
                break;
            }
            case 1: {  // fixed64
                if (offset + 8 > length) {
                    return false;
                }
                offset += 8;
                break;
            }
            case 2: {  // length-delimited
                std::uint64_t len;
                if (!readVarint(data, length, offset, len) || offset + len > length) {
                    return false;
                }
                if (fieldNumber == 2) {
                    out.payload.assign(data + offset, data + offset + len);
                }
                offset += static_cast<std::size_t>(len);
                break;
            }
            case 5: {  // fixed32
                if (offset + 4 > length) {
                    return false;
                }
                offset += 4;
                break;
            }
            default:
                return false;  // unsupported/invalid wire type
        }
    }
    return true;
}

bool decodePosition(const std::uint8_t* data, std::size_t length, PositionMessage& out) {
    std::size_t offset = 0;
    std::int32_t latI = 0;
    std::int32_t lonI = 0;
    bool haveLat = false;
    bool haveLon = false;

    while (offset < length) {
        std::uint64_t tag;
        if (!readVarint(data, length, offset, tag)) {
            return false;
        }
        const int fieldNumber = static_cast<int>(tag >> 3);
        const int wireType = static_cast<int>(tag & 0x7);

        switch (wireType) {
            case 0: {
                std::uint64_t value;
                if (!readVarint(data, length, offset, value)) {
                    return false;
                }
                break;
            }
            case 1: {
                if (offset + 8 > length) {
                    return false;
                }
                offset += 8;
                break;
            }
            case 2: {
                std::uint64_t len;
                if (!readVarint(data, length, offset, len) || offset + len > length) {
                    return false;
                }
                offset += static_cast<std::size_t>(len);
                break;
            }
            case 5: {
                if (offset + 4 > length) {
                    return false;
                }
                std::int32_t value;
                std::memcpy(&value, data + offset, 4);  // sfixed32 is little-endian on the wire
                if (fieldNumber == 1) {
                    latI = value;
                    haveLat = true;
                } else if (fieldNumber == 2) {
                    lonI = value;
                    haveLon = true;
                }
                offset += 4;
                break;
            }
            default:
                return false;
        }
    }

    if (haveLat && haveLon) {
        out.hasLatLon = true;
        out.latitude = static_cast<double>(latI) * 1e-7;
        out.longitude = static_cast<double>(lonI) * 1e-7;
    }
    return true;
}

}  // namespace cardmesh::mesh::pb
