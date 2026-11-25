#pragma once

#include <cstddef>
#include <string>

namespace cardmesh::mesh {

// Every region Meshtastic's firmware defines a confirmed, non-deprecated
// frequency band for (src/mesh/RadioInterface.cpp's RDEF table, protobufs'
// RegionCode enum), deliberately EXCLUDING:
//   - UA_868: marked deprecated in the protobuf since firmware 2.8.0.
//   - The amateur-radio-only ITU_* bands (144/220/430MHz ham allocations):
//     transmitting there legally requires an amateur radio license CardMesh
//     has no way to verify, unlike every other region here which is a
//     license-free ISM band for the general public.
//   - EU_N_868: identical freqStart/freqEnd to EU_868 (869.4-869.65MHz);
//     real Meshtastic gives it a different (narrower) default preset, but
//     CardMesh always computes its Primary channel as MediumFast (see
//     loraMediumFastPrimaryFrequencyMhz), so it would select the exact same
//     frequency as EU_868 anyway -- offering it as a second, identical
//     option would just be confusing.
// MeshCore has no published per-region frequency table of its own (checked
// against its firmware repo directly), so its worker reuses whichever of
// these frequencies matches the user's selected region too.
enum class LoraRegion {
    US,
    Eu433,
    Eu868,
    Eu866,
    China,
    Japan,
    Anz,
    Anz433,
    Russia,
    Korea,
    Taiwan,
    India,
    NewZealand865,
    Thailand,
    Ukraine433,
    Malaysia433,
    Malaysia919,
    Singapore923,
    Philippines433,
    Philippines868,
    Philippines915,
    Kazakhstan433,
    Kazakhstan863,
    Nepal865,
    Brazil902,
    Lora24Ghz,
    Count,
};

constexpr std::size_t kLoraRegionCount = static_cast<std::size_t>(LoraRegion::Count);

// Short display name for the Settings screen row, e.g. "US 915", "EU 868".
const char* loraRegionDisplayName(LoraRegion region);

// Stable identifier for config.json persistence, matching Meshtastic's own
// RegionCode enum names (e.g. "US", "EU_868", "MY_919") so the stored value
// stays meaningful outside CardMesh too.
const char* loraRegionConfigName(LoraRegion region);

// Reverse of loraRegionConfigName; falls back to LoraRegion::US for an
// empty, unrecognized, or (deliberately unsupported) "UA_868" value, same
// as any other unrecognized config.json field.
LoraRegion loraRegionFromConfigName(const std::string& name);

LoraRegion nextLoraRegion(LoraRegion region);

// This region's Primary channel frequency in MHz, computed the same way
// real Meshtastic firmware computes it for the MediumFast preset (see
// loraMediumFastPrimaryFrequencyMhz) -- used for both the direct Meshtastic
// and direct MeshCore backends, since MeshCore has no region table of its
// own to compute a different value from.
float loraRegionPrimaryFrequencyMhz(LoraRegion region);

}  // namespace cardmesh::mesh
