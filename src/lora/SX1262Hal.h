#pragma once

#include <cstdint>
#include <map>
#include <memory>

#include "Hal.h"
#include "LinuxGpio.h"
#include "LinuxSpi.h"

namespace cardmesh::lora {

// RadioLibHal implementation over this device's raw GPIO character device
// and spidev, for CardputerZero's SX1262 LoRa hat. Pin numbers below are
// BCM GPIO offsets on /dev/gpiochip0, taken from CardputerZero's own
// open-source Cap-LoRa-1262 reference app (MIT licensed):
//   RST = 26 (output, active high reset-release)
//   BUSY/GPIO = 22 (input, RadioLib's generic "gpio" pin)
//   IRQ/DIO1 = 23 (input)
// Chip select is handled by the kernel's spidev driver on each transfer,
// not toggled as a separate GPIO, so CS is passed as RADIOLIB_NC to RadioLib.
class SX1262Hal final : public RadioLibHal {
public:
    enum class PinNum : std::uint32_t {
        Irq = 23,
        Rst = 26,
        Busy = 22,
    };

    explicit SX1262Hal(LinuxSpiDevice& spi);

    void pinMode(std::uint32_t pin, std::uint32_t mode) override;
    void digitalWrite(std::uint32_t pin, std::uint32_t value) override;
    std::uint32_t digitalRead(std::uint32_t pin) override;
    void attachInterrupt(std::uint32_t interruptNum, void (*interruptCb)(void), std::uint32_t mode) override;
    void detachInterrupt(std::uint32_t interruptNum) override;
    void delay(RadioLibTime_t ms) override;
    void delayMicroseconds(RadioLibTime_t us) override;
    RadioLibTime_t millis() override;
    RadioLibTime_t micros() override;
    long pulseIn(std::uint32_t pin, std::uint32_t state, RadioLibTime_t timeout) override;
    void spiBegin() override;
    void spiBeginTransaction() override;
    void spiTransfer(std::uint8_t* out, std::size_t len, std::uint8_t* in) override;
    void spiEndTransaction() override;
    void spiEnd() override;

private:
    LinuxSpiDevice& spi_;
    std::map<std::uint32_t, LinuxGpioLine> outputs_;
    std::map<std::uint32_t, LinuxGpioLine> inputs_;
};

}  // namespace cardmesh::lora
