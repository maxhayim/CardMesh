#pragma once

#include <cstdint>
#include <string>

namespace cardmesh::lora {

class LinuxI2cBus {
public:
    LinuxI2cBus() = default;
    ~LinuxI2cBus();

    LinuxI2cBus(const LinuxI2cBus&) = delete;
    LinuxI2cBus& operator=(const LinuxI2cBus&) = delete;

    bool open(const std::string& devicePath);
    bool isOpen() const { return fd_ >= 0; }

    // Single write transaction.
    bool write(std::uint8_t address7, const std::uint8_t* data, std::size_t length);

    // Write then read in one transaction (repeated start, no STOP in
    // between) -- required for register-pointer reads on devices like the
    // PI4IOE5V6408 that would otherwise race with other bus clients.
    bool writeThenRead(std::uint8_t address7, const std::uint8_t* writeData, std::size_t writeLength,
                        std::uint8_t* readData, std::size_t readLength);

    void close();

private:
    int fd_ = -1;
};

}  // namespace cardmesh::lora
