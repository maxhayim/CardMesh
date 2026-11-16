#include "ChannelKey.h"

#include "MeshtasticCrypto.h"

namespace cardmesh::mesh {

namespace {

int decodeChar(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

}  // namespace

std::vector<std::uint8_t> base64Decode(const std::string& input) {
    std::vector<std::uint8_t> out;
    int buffer = 0;
    int bits = 0;
    for (char c : input) {
        if (c == '=' || c == '\n' || c == '\r') {
            continue;
        }
        const int value = decodeChar(c);
        if (value < 0) {
            return {};  // malformed input
        }
        buffer = (buffer << 6) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

std::vector<std::uint8_t> resolveChannelKey(const std::string& base64Psk) {
    const std::vector<std::uint8_t> decoded = base64Decode(base64Psk);
    if (decoded.empty()) {
        return {};
    }
    if (decoded.size() == 1) {
        return expandDefaultPsk(decoded[0]);
    }
    return decoded;
}

}  // namespace cardmesh::mesh
