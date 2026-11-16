#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cardmesh::lora {

// SPI mode 0 (CPOL=0, CPHA=0), 8 bits/word, MSB first -- matches the
// SX1262's requirements and the known-working CardputerZero Cap-LoRa-1262
// reference configuration.
class LinuxSpiDevice {
public:
    LinuxSpiDevice() = default;
    ~LinuxSpiDevice();

    LinuxSpiDevice(const LinuxSpiDevice&) = delete;
    LinuxSpiDevice& operator=(const LinuxSpiDevice&) = delete;

    bool open(const std::string& devicePath, std::uint32_t speedHz);
    bool isOpen() const { return fd_ >= 0; }

    // Full-duplex transfer; tx/rx may be null (treated as all-zero
    // write / discarded read respectively) but not both.
    bool transfer(const std::uint8_t* tx, std::uint8_t* rx, std::size_t length);

    void close();

private:
    int fd_ = -1;
    std::uint32_t speedHz_ = 0;
};

}  // namespace cardmesh::lora
