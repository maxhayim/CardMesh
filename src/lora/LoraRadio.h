#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "HatPower.h"
#include "LinuxI2c.h"
#include "LinuxSpi.h"
#include "Pi4ioExpander.h"
#include "SX1262Hal.h"

class Module;
class SX1262;

namespace cardmesh::lora {

struct RadioParams {
    float frequencyMhz;
    float bandwidthKhz;
    std::uint8_t spreadingFactor;
    std::uint8_t codingRate;
    std::uint8_t syncWord;
    int8_t outputPowerDbm;
    std::uint16_t preambleSymbols;
    float tcxoVoltage;
    float currentLimitMa;
};

// Raw SX1262 packet send/receive over CardputerZero's LoRa hat. This is the
// hardware bring-up layer only -- it moves raw bytes over the air with
// whatever modem parameters RadioParams specifies. It does not know about
// Meshtastic's packet framing or per-channel encryption; that's a separate
// layer built on top (see src/mesh/).
class LoraRadio final {
public:
    LoraRadio();
    ~LoraRadio();

    LoraRadio(const LoraRadio&) = delete;
    LoraRadio& operator=(const LoraRadio&) = delete;

    // Powers the hat, opens the I2C/SPI/GPIO handles, and brings the radio
    // up with the given parameters. Returns false (and leaves everything
    // cleanly shut down) on any failure.
    bool begin(const RadioParams& params);

    // Puts the radio in continuous-receive mode. Call once after begin();
    // re-call after a send() to resume listening.
    bool startReceive();

    // Transmits synchronously; the caller should startReceive() again
    // afterward if they want to keep listening.
    bool send(const std::uint8_t* data, std::size_t length);

    // Non-blocking: call periodically. Returns true and fills packet with
    // the decoded bytes when a new packet has fully arrived since the last
    // call.
    bool poll(std::vector<std::uint8_t>* packet, float* rssi = nullptr, float* snr = nullptr);

    void shutdown();

    bool isReady() const { return ready_; }
    const std::string& spiDevicePath() const { return spiDevicePath_; }
    const std::string& diagnostics() const { return diagnostics_; }

private:
    bool initHardware();

    HatPowerRails hatPower_;
    LinuxI2cBus i2c_;
    std::unique_ptr<Pi4ioExpander> pi4io_;
    std::unique_ptr<Pi4ioHatGate> pi4ioGate_;
    LinuxSpiDevice spi_;
    std::unique_ptr<SX1262Hal> hal_;
    std::unique_ptr<Module> module_;
    std::unique_ptr<SX1262> sx1262_;

    bool ready_ = false;
    std::string spiDevicePath_ = "/dev/spidev0.1";
    std::string diagnostics_;

    inline static std::atomic<bool> receivedFlag_{false};
    inline static std::atomic<bool> transmittedFlag_{false};
    static void onPacketReceived() noexcept;
    static void onPacketTransmitted() noexcept;
};

}  // namespace cardmesh::lora
