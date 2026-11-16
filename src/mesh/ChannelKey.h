#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cardmesh::mesh {

// Decodes a standard base64 string (RFC 4648, '+'/'/' alphabet, '=' padding
// optional). Returns an empty vector on malformed input.
std::vector<std::uint8_t> base64Decode(const std::string& input);

// Resolves a channel's base64-encoded PSK (as returned by MeshMonitor's
// /channels endpoint) into raw AES key bytes, per Meshtastic convention:
// a decoded length of 0 means encryption is disabled (returns empty), a
// decoded length of 1 is a "default PSK index" byte (expanded via
// expandDefaultPsk), and any other length is used as the raw AES-128/256
// key directly.
std::vector<std::uint8_t> resolveChannelKey(const std::string& base64Psk);

}  // namespace cardmesh::mesh
