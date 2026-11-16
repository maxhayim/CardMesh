#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cardmesh::mesh::pb {

// Minimal protobuf wire-format helpers -- just enough to encode/decode the
// Meshtastic `Data` message (mesh.proto), not a general protobuf library.

void appendVarint(std::vector<std::uint8_t>& out, std::uint64_t value);
void appendTag(std::vector<std::uint8_t>& out, int fieldNumber, int wireType);
void appendBytesField(std::vector<std::uint8_t>& out, int fieldNumber, const std::uint8_t* data, std::size_t length);
void appendVarintField(std::vector<std::uint8_t>& out, int fieldNumber, std::uint64_t value);

// Reads a varint starting at `offset`; advances `offset` past it. Returns
// false (leaving offset unspecified) if the buffer is exhausted first.
bool readVarint(const std::uint8_t* data, std::size_t length, std::size_t& offset, std::uint64_t& value);

// Data message (mesh.proto): only the fields CardMesh actually uses.
struct DataMessage {
    std::uint32_t portnum = 0;
    std::vector<std::uint8_t> payload;
};

std::vector<std::uint8_t> encodeData(const DataMessage& msg);

// Parses field 1 (portnum, varint) and field 2 (payload, bytes); any other
// field is skipped generically so unrecognized/future fields don't break
// decoding.
bool decodeData(const std::uint8_t* data, std::size_t length, DataMessage& out);

// Position message (mesh.proto): only latitude_i/longitude_i (fields 1/2,
// sfixed32, degrees * 1e7), for feeding directly-heard node positions into
// the map.
struct PositionMessage {
    bool hasLatLon = false;
    double latitude = 0.0;
    double longitude = 0.0;
};

bool decodePosition(const std::uint8_t* data, std::size_t length, PositionMessage& out);

}  // namespace cardmesh::mesh::pb
