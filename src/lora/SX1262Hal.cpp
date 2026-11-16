#include "SX1262Hal.h"

#include <ctime>
#include <cstring>
#include <unistd.h>

namespace cardmesh::lora {

namespace {
constexpr const char* kGpioChip = "/dev/gpiochip0";
}

SX1262Hal::SX1262Hal(LinuxSpiDevice& spi)
    : RadioLibHal(/* input */ 0, /* output */ 1, /* low */ 0, /* high */ 1, /* rising */ 1, /* falling */ 2),
      spi_(spi) {}

void SX1262Hal::pinMode(std::uint32_t pin, std::uint32_t mode) {
    if (pin == RADIOLIB_NC) {
        return;
    }
    if (mode == GpioModeOutput) {
        if (outputs_.find(pin) != outputs_.end()) {
            return;
        }
        LinuxGpioLine line;
        if (line.requestOutput(kGpioChip, pin, false, "cardmesh-lora")) {
            outputs_.emplace(pin, std::move(line));
        }
        return;
    }
    if (inputs_.find(pin) != inputs_.end()) {
        return;
    }
    LinuxGpioLine line;
    if (line.requestInput(kGpioChip, pin, "cardmesh-lora")) {
        inputs_.emplace(pin, std::move(line));
    }
}

void SX1262Hal::digitalWrite(std::uint32_t pin, std::uint32_t value) {
    if (pin == RADIOLIB_NC) {
        return;
    }
    const auto found = outputs_.find(pin);
    if (found != outputs_.end()) {
        found->second.set(value != 0);
    }
}

std::uint32_t SX1262Hal::digitalRead(std::uint32_t pin) {
    if (pin == RADIOLIB_NC) {
        return 0;
    }
    const auto found = inputs_.find(pin);
    if (found == inputs_.end()) {
        return 0;
    }
    bool high = false;
    return found->second.get(&high) && high ? 1U : 0U;
}

void SX1262Hal::attachInterrupt(std::uint32_t, void (*)(void), std::uint32_t) {
    // Not used: the radio is serviced by polling getIrqFlags(), matching
    // the CardputerZero reference app's approach.
}

void SX1262Hal::detachInterrupt(std::uint32_t) {}

void SX1262Hal::delay(RadioLibTime_t ms) { ::usleep(static_cast<useconds_t>(ms) * 1000U); }

void SX1262Hal::delayMicroseconds(RadioLibTime_t us) { ::usleep(static_cast<useconds_t>(us)); }

RadioLibTime_t SX1262Hal::millis() {
    struct timespec time {};
    ::clock_gettime(CLOCK_MONOTONIC, &time);
    return static_cast<RadioLibTime_t>(time.tv_sec * 1000ULL + time.tv_nsec / 1000000ULL);
}

RadioLibTime_t SX1262Hal::micros() {
    struct timespec time {};
    ::clock_gettime(CLOCK_MONOTONIC, &time);
    return static_cast<RadioLibTime_t>(time.tv_sec * 1000000ULL + time.tv_nsec / 1000ULL);
}

long SX1262Hal::pulseIn(std::uint32_t, std::uint32_t, RadioLibTime_t) { return 0; }

void SX1262Hal::spiBegin() {}

void SX1262Hal::spiBeginTransaction() {}

void SX1262Hal::spiTransfer(std::uint8_t* out, std::size_t len, std::uint8_t* in) {
    if (len == 0) {
        return;
    }
    if (!spi_.transfer(out, in, len) && in != nullptr) {
        std::memset(in, 0, len);
    }
}

void SX1262Hal::spiEndTransaction() {}

void SX1262Hal::spiEnd() {}

}  // namespace cardmesh::lora
