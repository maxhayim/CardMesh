#pragma once

#include <cstdint>
#include <string>

namespace cardmesh::lora {

// A single requested line on a Linux GPIO character-device chip
// (/dev/gpiochipN), via the v2 cdev ioctl API (GPIO_V2_GET_LINE_IOCTL).
class LinuxGpioLine {
public:
    LinuxGpioLine() = default;
    ~LinuxGpioLine();

    LinuxGpioLine(const LinuxGpioLine&) = delete;
    LinuxGpioLine& operator=(const LinuxGpioLine&) = delete;
    LinuxGpioLine(LinuxGpioLine&& other) noexcept;
    LinuxGpioLine& operator=(LinuxGpioLine&& other) noexcept;

    bool requestOutput(const std::string& chipPath, std::uint32_t offset, bool initialHigh,
                        const std::string& consumer);
    bool requestInput(const std::string& chipPath, std::uint32_t offset, const std::string& consumer);

    bool isOpen() const { return lineFd_ >= 0; }

    // Returns false on ioctl failure; result is only meaningful when this
    // returns true.
    bool get(bool* high) const;
    bool set(bool high);

    void close();

private:
    int lineFd_ = -1;
};

}  // namespace cardmesh::lora
