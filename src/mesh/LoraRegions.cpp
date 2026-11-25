#include "LoraRegions.h"

#include <cstdint>

namespace cardmesh::mesh {

namespace {

// meshtastic/firmware src/mesh/RadioInterface.cpp: uint32_t hash(const char*). djb2.
std::uint32_t djb2(const char* str) {
    std::uint32_t hash = 5381;
    int c;
    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    return hash;
}

// MediumFast (the actual preset this app always uses, per the user --
// NOT the more commonly-cited LongFast default) Primary channel frequency,
// in MHz, for a region spanning [freqStartMhz, freqEndMhz):
// freqStart + bw/2 + (djb2("MediumFast") % numFreqSlots) * bw, with
// bw = 0.25 MHz (MediumFast's 250kHz bandwidth, same as LongFast -- only
// the hashed name, and so the resulting slot, differs) and
// numFreqSlots = round((freqEnd - freqStart) / bw), clamped to at least 1
// for a region narrower than one slot. Matches
// RadioInterface::applyModemConfig()'s default (non-override) case for
// any region, not just US915.
float loraMediumFastPrimaryFrequencyMhz(float freqStartMhz, float freqEndMhz) {
    constexpr float kBwMhz = 0.25f;
    std::uint32_t numFreqSlots =
        static_cast<std::uint32_t>((freqEndMhz - freqStartMhz) / kBwMhz + 0.5f);
    if (numFreqSlots == 0) {
        numFreqSlots = 1;  // a region narrower than one 250kHz slot still gets slot 0
    }
    const std::uint32_t slot = djb2("MediumFast") % numFreqSlots;
    return freqStartMhz + (kBwMhz / 2.0f) + static_cast<float>(slot) * kBwMhz;
}

struct RegionSpec {
    LoraRegion region;
    const char* displayName;
    const char* configName;  // matches Meshtastic's protobuf RegionCode name
    float freqStartMhz;
    float freqEndMhz;
};

// freqStart/freqEnd for every row: read verbatim from the RDEF(...) table
// in meshtastic/firmware's src/mesh/RadioInterface.cpp (develop branch, see
// LoraRegions.h for which regions were deliberately left out and why).
constexpr RegionSpec kRegions[kLoraRegionCount] = {
    {LoraRegion::US, "US 915", "US", 902.0f, 928.0f},
    {LoraRegion::Eu433, "EU 433", "EU_433", 433.0f, 434.0f},
    {LoraRegion::Eu868, "EU 868", "EU_868", 869.4f, 869.65f},
    {LoraRegion::Eu866, "EU 866", "EU_866", 865.6f, 867.6f},
    {LoraRegion::China, "China 470", "CN", 470.0f, 510.0f},
    {LoraRegion::Japan, "Japan 920", "JP", 920.5f, 923.5f},
    {LoraRegion::Anz, "AU/NZ/BR 915", "ANZ", 915.0f, 928.0f},
    {LoraRegion::Anz433, "AU/NZ 433", "ANZ_433", 433.05f, 434.79f},
    {LoraRegion::Russia, "Russia 868", "RU", 868.7f, 869.2f},
    {LoraRegion::Korea, "Korea 920", "KR", 920.0f, 923.0f},
    {LoraRegion::Taiwan, "Taiwan 920", "TW", 920.0f, 925.0f},
    {LoraRegion::India, "India 865", "IN", 865.0f, 867.0f},
    {LoraRegion::NewZealand865, "New Zealand 865", "NZ_865", 864.0f, 868.0f},
    {LoraRegion::Thailand, "Thailand 920", "TH", 920.0f, 925.0f},
    {LoraRegion::Ukraine433, "Ukraine 433", "UA_433", 433.0f, 434.7f},
    {LoraRegion::Malaysia433, "Malaysia 433", "MY_433", 433.0f, 435.0f},
    {LoraRegion::Malaysia919, "Malaysia 919", "MY_919", 919.0f, 924.0f},
    {LoraRegion::Singapore923, "Singapore 923", "SG_923", 917.0f, 925.0f},
    {LoraRegion::Philippines433, "Philippines 433", "PH_433", 433.0f, 434.7f},
    {LoraRegion::Philippines868, "Philippines 868", "PH_868", 868.0f, 869.4f},
    {LoraRegion::Philippines915, "Philippines 915", "PH_915", 915.0f, 918.0f},
    {LoraRegion::Kazakhstan433, "Kazakhstan 433", "KZ_433", 433.075f, 434.775f},
    {LoraRegion::Kazakhstan863, "Kazakhstan 863", "KZ_863", 863.0f, 868.0f},
    {LoraRegion::Nepal865, "Nepal 865", "NP_865", 865.0f, 868.0f},
    {LoraRegion::Brazil902, "Brazil 902", "BR_902", 902.0f, 907.5f},
    {LoraRegion::Lora24Ghz, "2.4GHz (worldwide)", "LORA_24", 2400.0f, 2483.5f},
};

const RegionSpec& specFor(LoraRegion region) {
    const auto index = static_cast<std::size_t>(region);
    if (index >= kLoraRegionCount) {
        return kRegions[static_cast<std::size_t>(LoraRegion::US)];
    }
    return kRegions[index];
}

}  // namespace

const char* loraRegionDisplayName(LoraRegion region) { return specFor(region).displayName; }

const char* loraRegionConfigName(LoraRegion region) { return specFor(region).configName; }

LoraRegion loraRegionFromConfigName(const std::string& name) {
    for (const RegionSpec& spec : kRegions) {
        if (name == spec.configName) {
            return spec.region;
        }
    }
    return LoraRegion::US;
}

LoraRegion nextLoraRegion(LoraRegion region) {
    const auto index = static_cast<std::size_t>(region);
    const std::size_t next = (index + 1) % kLoraRegionCount;
    return kRegions[next].region;
}

float loraRegionPrimaryFrequencyMhz(LoraRegion region) {
    const RegionSpec& spec = specFor(region);
    return loraMediumFastPrimaryFrequencyMhz(spec.freqStartMhz, spec.freqEndMhz);
}

}  // namespace cardmesh::mesh
