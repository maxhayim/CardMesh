#include "MeshCoreWorker.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iterator>

#include "mesh/MeshCoreCrypto.h"
#include "mesh/MeshCorePacketCodec.h"

namespace cardmesh::ui {

namespace {

using cardmesh::mesh::KnownMeshCoreChannel;

// This device's MeshCore display name, matching the project's planned
// device-naming convention (Meshtastic device: "MTMX CP0M5", MeshCore
// device: "MCMX CP0M5").
constexpr const char* kDeviceName = "MCMX CP0M5";

KnownMeshCoreChannel makePublicChannel() {
    KnownMeshCoreChannel channel;
    channel.name = "Public";
    std::copy(std::begin(cardmesh::mesh::kMeshCorePublicChannelSecret),
              std::end(cardmesh::mesh::kMeshCorePublicChannelSecret), channel.secret);
    return channel;
}

// Splits a decoded "<senderName>: <text>" body into its two parts. MeshCore
// always prefixes group text with the sender name and ": ", per the
// firmware's own send-side formatting.
void splitSenderAndText(const std::string& body, std::string& senderName, std::string& text) {
    const auto pos = body.find(": ");
    if (pos == std::string::npos) {
        senderName = "?";
        text = body;
        return;
    }
    senderName = body.substr(0, pos);
    text = body.substr(pos + 2);
}

}  // namespace

MeshCoreWorker::MeshCoreWorker(MeshCoreMessagesModel& messages) : messages_(messages) {
    channels_.push_back(makePublicChannel());
}

MeshCoreWorker::~MeshCoreWorker() { stop(); }

void MeshCoreWorker::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&MeshCoreWorker::run, this);
}

void MeshCoreWorker::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void MeshCoreWorker::setChannels(std::vector<KnownMeshCoreChannel> channels) {
    std::lock_guard<std::mutex> lock(channelsMutex_);
    channels_ = std::move(channels);
    channels_.push_back(makePublicChannel());
}

bool MeshCoreWorker::sendText(const std::string& channelName, const std::string& text) {
    if (findChannel(channelName, channelsSnapshot()) == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(outgoingMutex_);
    outgoingQueue_.push_back({channelName, text});
    return true;
}

std::vector<KnownMeshCoreChannel> MeshCoreWorker::channelsSnapshot() {
    std::lock_guard<std::mutex> lock(channelsMutex_);
    return channels_;
}

std::vector<std::string> MeshCoreWorker::channelNames() {
    std::vector<std::string> names;
    for (const auto& channel : channelsSnapshot()) {
        names.push_back(channel.name);
    }
    return names;
}

const KnownMeshCoreChannel* MeshCoreWorker::findChannel(const std::string& name,
                                                         const std::vector<KnownMeshCoreChannel>& channels) {
    for (const auto& channel : channels) {
        if (channel.name == name) {
            return &channel;
        }
    }
    return nullptr;
}

void MeshCoreWorker::run() {
    using namespace std::chrono_literals;

    cardmesh::lora::RadioParams params;
    params.frequencyMhz = 915.0f;          // MeshCore companion_radio default (LORA_FREQ)
    params.bandwidthKhz = 250.0f;          // LORA_BW
    params.spreadingFactor = 10;           // LORA_SF
    params.codingRate = 5;                 // LORA_CR
    params.syncWord = 0x12;                // RADIOLIB_SX126X_SYNC_WORD_PRIVATE
    params.outputPowerDbm = 20;            // LORA_TX_POWER
    params.preambleSymbols = 16;           // companion_radio's begin() call
    params.tcxoVoltage = 3.0f;
    params.currentLimitMa = 140.0f;

    const bool ok = radio_.begin(params);
    {
        std::lock_guard<std::mutex> lock(messages_.mutex);
        messages_.radioReady = ok;
        messages_.statusMessage = ok ? "MESHCORE: READY" : ("MESHCORE: FAILED - " + radio_.diagnostics());
    }
    if (!ok) {
        running_ = false;
        return;
    }
    radio_.startReceive();

    while (running_) {
        bool sentSomething = false;
        {
            std::lock_guard<std::mutex> lock(outgoingMutex_);
            sentSomething = !outgoingQueue_.empty();
        }
        if (sentSomething) {
            sendNext();
        }
        pollIncoming();
        std::this_thread::sleep_for(100ms);
    }

    radio_.shutdown();
}

void MeshCoreWorker::sendNext() {
    OutgoingMessage outgoing;
    {
        std::lock_guard<std::mutex> lock(outgoingMutex_);
        if (outgoingQueue_.empty()) {
            return;
        }
        outgoing = outgoingQueue_.front();
        outgoingQueue_.pop_front();
    }

    const auto channels = channelsSnapshot();
    const KnownMeshCoreChannel* channel = findChannel(outgoing.channelName, channels);
    if (channel == nullptr) {
        return;  // channel was removed by a setChannels() call since this was queued
    }

    const std::string& text = outgoing.text;
    const auto timestamp = static_cast<std::uint32_t>(std::time(nullptr));
    const auto groupTextPayload =
        cardmesh::mesh::encodeMeshCoreGroupText(channel->secret, kDeviceName, text, timestamp);
    const auto frame = cardmesh::mesh::encodeMeshCoreFloodPacket(cardmesh::mesh::kMeshCorePayloadTypeGrpTxt, groupTextPayload);

    const bool sent = radio_.send(frame.data(), frame.size());
    radio_.startReceive();

    if (sent) {
        MeshCoreMessageEntry entry;
        entry.senderName = kDeviceName;
        entry.text = text;
        entry.timestamp = static_cast<std::int64_t>(timestamp);
        entry.outgoing = true;

        std::lock_guard<std::mutex> lock(messages_.mutex);
        messages_.messages.push_back(std::move(entry));
    }
}

void MeshCoreWorker::pollIncoming() {
    std::vector<std::uint8_t> packet;
    float rssi = 0.0f;
    float snr = 0.0f;
    if (!radio_.poll(&packet, &rssi, &snr)) {
        return;
    }

    cardmesh::mesh::MeshCoreRawPacket raw;
    if (!cardmesh::mesh::decodeMeshCoreRawPacket(packet.data(), packet.size(), raw)) {
        return;
    }
    if (raw.payloadType() != cardmesh::mesh::kMeshCorePayloadTypeGrpTxt) {
        return;  // not a channel text message; ignore (DMs, adverts, acks, etc. not yet supported)
    }

    // Try every known channel's secret -- decodeMeshCoreGroupText rejects
    // on a mismatched channel hash (the payload's first byte) before even
    // attempting the MAC/decrypt, so this is cheap even with several
    // channels known.
    cardmesh::mesh::DecodedMeshCoreGroupText decoded;
    bool matched = false;
    for (const auto& channel : channelsSnapshot()) {
        if (cardmesh::mesh::decodeMeshCoreGroupText(channel.secret, raw.payload.data(), raw.payload.size(), decoded)) {
            matched = true;
            break;
        }
    }
    if (!matched) {
        return;  // not on any known channel, or MAC failed
    }

    MeshCoreMessageEntry entry;
    splitSenderAndText(decoded.body, entry.senderName, entry.text);
    entry.timestamp = static_cast<std::int64_t>(decoded.timestamp);
    entry.outgoing = false;
    entry.rssi = rssi;
    entry.snr = snr;

    std::lock_guard<std::mutex> lock(messages_.mutex);
    messages_.messages.push_back(std::move(entry));
}

}  // namespace cardmesh::ui
