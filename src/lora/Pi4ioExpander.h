#pragma once

#include <cstdint>
#include <optional>

#include "LinuxI2c.h"

namespace cardmesh::lora {

// PI4IOE5V6408 8-bit I/O expander, register map per the datasheet
// (https://www.diodes.com/assets/Datasheets/PI4IOE5V6408.pdf). Only the P0
// snapshot/restore sequence CardputerZero's LoRa hat needs is modeled here
// -- see Pi4ioHatGate for what it's used for.
class Pi4ioExpander {
public:
    explicit Pi4ioExpander(LinuxI2cBus& bus, std::uint8_t address7 = 0x43) : bus_(bus), address_(address7) {}

    // Reads register 0x01 and checks the device acknowledged.
    bool begin();

    std::optional<std::uint8_t> readRegister8(std::uint8_t reg);
    bool writeRegister8(std::uint8_t reg, std::uint8_t value);

private:
    LinuxI2cBus& bus_;
    std::uint8_t address_;
};

// Snapshots the P0 bit of the output/polarity/direction registers, drives
// P0 as a non-inverted output held high while the radio is in use, and
// restores the original register state on shutdown -- this is CardputerZero's
// LoRa hat gate signal; leaving it altered after exit affects the physical
// hat connector for whatever runs next. Mirrors Cap-LoRa-1262's
// cp0_lora_pi4io_controller exactly.
class Pi4ioHatGate {
public:
    explicit Pi4ioHatGate(Pi4ioExpander& expander) : expander_(expander) {}

    bool enable();
    void restore();

private:
    Pi4ioExpander& expander_;
    bool active_ = false;
    std::uint8_t savedOutput_ = 0;
    std::uint8_t savedPolarity_ = 0;
    std::uint8_t savedConfig_ = 0;
};

}  // namespace cardmesh::lora
