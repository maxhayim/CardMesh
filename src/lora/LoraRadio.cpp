#include "LoraRadio.h"

#include <cstdio>

#include "RadioLib.h"

namespace cardmesh::lora {

namespace {
constexpr const char* kSpiCandidates[] = {"/dev/spidev0.1", "/dev/spidev0.0"};
}

void LoraRadio::onPacketReceived() noexcept { receivedFlag_.store(true, std::memory_order_release); }
void LoraRadio::onPacketTransmitted() noexcept { transmittedFlag_.store(true, std::memory_order_release); }

LoraRadio::LoraRadio() = default;
LoraRadio::~LoraRadio() { shutdown(); }

bool LoraRadio::initHardware() {
    if (!hatPower_.enable()) {
        diagnostics_ = "failed to enable hat 5V rails";
        return false;
    }

    if (!i2c_.open("/dev/i2c-1")) {
        diagnostics_ = "failed to open /dev/i2c-1";
        hatPower_.restore();
        return false;
    }
    // Matches Cap-LoRa-1262's InitHard(): PI4IO is best-effort diagnostics,
    // not a precondition for the SX1262 itself (which only needs SPI/GPIO).
    pi4io_ = std::make_unique<Pi4ioExpander>(i2c_);
    if (!pi4io_->begin()) {
        diagnostics_ = "PI4IO scan unavailable; continuing with direct SX1262 GPIO/SPI";
        pi4io_.reset();
    } else {
        pi4ioGate_ = std::make_unique<Pi4ioHatGate>(*pi4io_);
        if (!pi4ioGate_->enable()) {
            diagnostics_ = "PI4IO hat gate unavailable; continuing with direct SX1262 GPIO/SPI";
            pi4ioGate_.reset();
        }
    }

    bool spiOpened = false;
    for (const char* candidate : kSpiCandidates) {
        if (spi_.open(candidate, 1000000)) {
            spiDevicePath_ = candidate;
            spiOpened = true;
            break;
        }
    }
    if (!spiOpened) {
        diagnostics_ = "failed to open any SPI candidate device";
        pi4ioGate_->restore();
        hatPower_.restore();
        return false;
    }

    return true;
}

bool LoraRadio::begin(const RadioParams& params) {
    if (ready_) {
        return true;
    }
    if (!initHardware()) {
        return false;
    }

    hal_ = std::make_unique<SX1262Hal>(spi_);
    module_ = std::make_unique<Module>(hal_.get(), RADIOLIB_NC, static_cast<std::uint32_t>(SX1262Hal::PinNum::Irq),
                                       static_cast<std::uint32_t>(SX1262Hal::PinNum::Rst),
                                       static_cast<std::uint32_t>(SX1262Hal::PinNum::Busy));
    module_->spiConfig.timeout = 100;
    sx1262_ = std::make_unique<SX1262>(module_.get());

    const int16_t result = sx1262_->begin(params.frequencyMhz, params.bandwidthKhz, params.spreadingFactor,
                                          params.codingRate, params.syncWord, params.outputPowerDbm,
                                          params.preambleSymbols, params.tcxoVoltage, false);
    if (result != RADIOLIB_ERR_NONE) {
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "SX1262 begin() failed, code=%d", result);
        diagnostics_ = buffer;
        shutdown();
        return false;
    }
    if (sx1262_->setCurrentLimit(params.currentLimitMa) != RADIOLIB_ERR_NONE ||
        sx1262_->setDio2AsRfSwitch(true) != RADIOLIB_ERR_NONE) {
        diagnostics_ = "SX1262 setCurrentLimit/setDio2AsRfSwitch failed";
        shutdown();
        return false;
    }
    sx1262_->setPacketReceivedAction(&LoraRadio::onPacketReceived);
    sx1262_->setPacketSentAction(&LoraRadio::onPacketTransmitted);

    diagnostics_ = "SX1262 ready";
    ready_ = true;
    return true;
}

bool LoraRadio::startReceive() {
    if (!ready_ || !sx1262_) {
        return false;
    }
    return sx1262_->startReceive() == RADIOLIB_ERR_NONE;
}

bool LoraRadio::send(const std::uint8_t* data, std::size_t length) {
    if (!ready_ || !sx1262_ || data == nullptr || length == 0) {
        return false;
    }
    if (sx1262_->standby() != RADIOLIB_ERR_NONE) {
        return false;
    }
    transmittedFlag_.store(false, std::memory_order_release);
    if (sx1262_->transmit(data, length) != RADIOLIB_ERR_NONE) {
        return false;
    }
    return true;
}

bool LoraRadio::poll(std::vector<std::uint8_t>* packet, float* rssi, float* snr) {
    if (!ready_ || !sx1262_ || packet == nullptr) {
        return false;
    }
    const std::uint32_t irqFlags = sx1262_->getIrqFlags();
    const bool rxDone =
        receivedFlag_.exchange(false, std::memory_order_acq_rel) || ((irqFlags & RADIOLIB_SX126X_IRQ_RX_DONE) != 0);
    if (!rxDone) {
        return false;
    }
    const std::size_t length = sx1262_->getPacketLength();
    if (length == 0) {
        return false;
    }
    packet->assign(length, 0);
    if (sx1262_->readData(packet->data(), length) != RADIOLIB_ERR_NONE) {
        packet->clear();
        return false;
    }
    if (rssi != nullptr) {
        *rssi = sx1262_->getRSSI();
    }
    if (snr != nullptr) {
        *snr = sx1262_->getSNR();
    }
    return true;
}

void LoraRadio::shutdown() {
    if (sx1262_) {
        sx1262_->standby();
        sx1262_->sleep();
    }
    sx1262_.reset();
    module_.reset();
    hal_.reset();
    spi_.close();
    if (pi4ioGate_) {
        pi4ioGate_->restore();
    }
    pi4ioGate_.reset();
    pi4io_.reset();
    i2c_.close();
    hatPower_.restore();
    ready_ = false;
}

}  // namespace cardmesh::lora
