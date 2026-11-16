#pragma once

#include <cstdint>
#include <vector>

namespace cardmesh::mesh {

// Meshtastic's published 16-byte "default" channel PSK family (public,
// not a secret -- every stock Meshtastic node ships with this constant).
// A channel PSK of size 1 is a "default PSK index": byte value N selects
// this key with its last byte bumped by (N - 1); index 1 (the "AQ=="
// convention) uses it unmodified.
std::vector<std::uint8_t> expandDefaultPsk(std::uint8_t index);

// Builds the 16-byte AES-CTR nonce/IV Meshtastic uses for channel (PSK)
// encryption: packetId (8 bytes LE, packetId is actually only 32 bits so
// the top 4 are zero) || fromNode (4 bytes LE) || zero (4 bytes).
void buildNonce(std::uint32_t packetId, std::uint32_t fromNode, std::uint8_t nonce[16]);

// En/decrypts `data` in place with the channel's resolved key (16 or 32
// raw bytes) using AES-CTR. Symmetric: the same call does both directions.
bool channelCrypt(const std::uint8_t* key, std::size_t keyLength, std::uint32_t packetId, std::uint32_t fromNode,
                   std::uint8_t* data, std::size_t length);

// The 0-255 channel hash embedded in every packet's header, letting a
// receiver guess which locally-known channel an encrypted packet belongs
// to: xorHash(channel name bytes) ^ xorHash(resolved key bytes).
std::uint8_t channelHash(const char* name, const std::uint8_t* key, std::size_t keyLength);

// US915 MediumFast (the actual preset this network uses, per the user --
// NOT the more commonly-cited LongFast default) center frequency, in MHz,
// for the Primary channel's default frequency slot:
// 902.125 + (djb2("MediumFast") % 104) * 0.25. Bandwidth (250kHz) and so
// numFreqSlots are the same as LongFast; only the hashed name (and thus
// the resulting slot) and the spreading factor (9, not 11) differ. This
// matches RadioInterface::applyModemConfig()'s default (non-override) case.
float us915MediumFastPrimaryFrequencyMhz();

}  // namespace cardmesh::mesh
