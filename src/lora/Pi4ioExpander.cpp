#include "Pi4ioExpander.h"

#include <unistd.h>

namespace cardmesh::lora {

bool Pi4ioExpander::begin() {
    const auto id = readRegister8(0x01);
    return id.has_value() && *id != 0;
}

std::optional<std::uint8_t> Pi4ioExpander::readRegister8(std::uint8_t reg) {
    std::uint8_t value = 0;
    if (bus_.writeThenRead(address_, &reg, 1, &value, 1)) {
        return value;
    }
    ::usleep(2000);
    if (!bus_.writeThenRead(address_, &reg, 1, &value, 1)) {
        return std::nullopt;
    }
    return value;
}

bool Pi4ioExpander::writeRegister8(std::uint8_t reg, std::uint8_t value) {
    const std::uint8_t payload[2] = {reg, value};
    // The expander occasionally NACKs a write immediately following another
    // transaction; one short-delay retry clears it reliably in practice.
    if (bus_.write(address_, payload, 2)) {
        return true;
    }
    ::usleep(2000);
    return bus_.write(address_, payload, 2);
}

namespace {
constexpr std::uint8_t kRegOutput = 0x01;
constexpr std::uint8_t kRegPolarity = 0x02;
constexpr std::uint8_t kRegConfig = 0x03;
constexpr std::uint8_t kP0Bit = 0x01;
}  // namespace

bool Pi4ioHatGate::enable() {
    if (active_) {
        return true;
    }
    const auto output = expander_.readRegister8(kRegOutput);
    const auto polarity = expander_.readRegister8(kRegPolarity);
    const auto config = expander_.readRegister8(kRegConfig);
    if (!output.has_value() || !polarity.has_value() || !config.has_value()) {
        return false;
    }
    savedOutput_ = *output;
    savedPolarity_ = *polarity;
    savedConfig_ = *config;

    const std::uint8_t newPolarity = static_cast<std::uint8_t>(savedPolarity_ & ~kP0Bit);
    const std::uint8_t newOutput = static_cast<std::uint8_t>(savedOutput_ | kP0Bit);
    const std::uint8_t newConfig = static_cast<std::uint8_t>(savedConfig_ & ~kP0Bit);

    if (!expander_.writeRegister8(kRegPolarity, newPolarity) || !expander_.writeRegister8(kRegOutput, newOutput) ||
        !expander_.writeRegister8(kRegConfig, newConfig)) {
        // Best-effort: don't leave a partial write in place.
        expander_.writeRegister8(kRegConfig, savedConfig_);
        expander_.writeRegister8(kRegOutput, savedOutput_);
        expander_.writeRegister8(kRegPolarity, savedPolarity_);
        return false;
    }

    active_ = true;
    return true;
}

void Pi4ioHatGate::restore() {
    if (!active_) {
        return;
    }
    // This controller owns only the P0 bit of each register; preserve
    // whatever other bits another I2C client may have changed meanwhile.
    const struct {
        std::uint8_t reg;
        std::uint8_t saved;
    } registers[] = {
        {kRegOutput, savedOutput_},
        {kRegPolarity, savedPolarity_},
        {kRegConfig, savedConfig_},
    };
    for (const auto& item : registers) {
        const auto current = expander_.readRegister8(item.reg);
        if (!current.has_value()) {
            continue;
        }
        const std::uint8_t restored = static_cast<std::uint8_t>((*current & ~kP0Bit) | (item.saved & kP0Bit));
        expander_.writeRegister8(item.reg, restored);
    }
    active_ = false;
}

}  // namespace cardmesh::lora
