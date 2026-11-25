#include "MeshtasticWorker.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <random>
#include <sstream>
#include <utility>

#include "mesh/ChannelKey.h"
#include "mesh/LoraRegions.h"
#include "mesh/MeshtasticCrypto.h"
#include "storage/Settings.h"

namespace cardmesh::ui {

namespace {

using cardmesh::mesh::DecodedPacket;
using cardmesh::mesh::KnownChannel;
using cardmesh::mesh::PacketHeader;

std::uint32_t randomU32() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<std::uint32_t> dist;
    return dist(gen);
}

// Matches ui_main.cpp's logMs() -- both start their own clock at first use,
// so timestamps are only meaningful for ordering lines from the SAME
// process, which is all these diagnostics need.
long long nowMs() {
    // A raw steady_clock reading (no per-TU "start" epoch subtracted) is a
    // single, process-wide monotonic timeline shared by ui_main.cpp,
    // MeshtasticWorker.cpp, and MeshCoreWorker.cpp -- subtracting an
    // independent "first call in this translation unit" epoch per file (the
    // earlier version of this helper) silently produced three unrelated
    // clocks, which made cross-file [nav]/[worker] gaps meaningless.
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Printable, not semantic (there's no portable "is this the main thread"
// check) -- but consistent within one run, so two log lines sharing this
// value ran on the same thread, which is what matters for telling "the
// worker thread is slow" apart from "the calling (likely main) thread
// blocked inside start()/stop()".
std::string threadTag() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

// Avoid 0 (reserved/"unset") and the broadcast address for a node's own identity.
std::uint32_t generateNodeNum() {
    std::uint32_t id;
    do {
        id = randomU32();
    } while (id == 0 || id == PacketHeader::kBroadcast);
    return id;
}

// This network's actual primary channel (per the user, not the more
// commonly-cited LongFast default): MediumFast preset, same public
// default PSK convention.
KnownChannel makePrimaryChannel() {
    KnownChannel channel;
    channel.name = "MediumFast";
    channel.key = cardmesh::mesh::expandDefaultPsk(1);
    return channel;
}

}  // namespace

MeshtasticWorker::MeshtasticWorker(storage::AppSettings& settings, LoraMessagesModel& messages, MapModel& mapModel)
    : settings_(settings), messages_(messages), mapModel_(mapModel) {
    channels_.push_back(makePrimaryChannel());
}

MeshtasticWorker::~MeshtasticWorker() { stop(); }

void MeshtasticWorker::start() {
    if (running_.exchange(true)) {
        std::fprintf(stderr, "[worker %6lldms t=%s] MeshtasticWorker::start() ignored, already running\n", nowMs(), threadTag().c_str());
        return;
    }
    std::fprintf(stderr, "[worker %6lldms t=%s] MeshtasticWorker::start()\n", nowMs(), threadTag().c_str());
    if (settings_.loraNodeNum == 0) {
        settings_.loraNodeNum = generateNodeNum();
        storage::Settings::save(settings_);
    }
    // run() can finish and clear running_ on its own (radio_.begin() failing
    // fast) without anyone ever calling stop() in between -- if that
    // happened, thread_ is still joinable from the previous run, and
    // move-assigning a new std::thread into an already-joinable thread_
    // calls std::terminate() and aborts the whole process. Confirmed live:
    // this is exactly what crashed when re-entering a screen whose worker's
    // last begin() had failed fast.
    if (thread_.joinable()) {
        thread_.join();
    }
    thread_ = std::thread(&MeshtasticWorker::run, this);
}

void MeshtasticWorker::stop() {
    // Don't gate the join on running_: run() clears running_ itself when
    // radio_.begin() fails (see below), so by the time stop() is next
    // called running_ may already be false even though thread_ is still
    // joinable. Destroying a joinable std::thread calls std::terminate()
    // and aborts the whole process -- always join whatever's left instead
    // of relying on the exchange() to tell us there's something to join.
    std::fprintf(stderr, "[worker %6lldms t=%s] MeshtasticWorker::stop() (joinable=%d)\n", nowMs(), threadTag().c_str(), thread_.joinable());
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    std::fprintf(stderr, "[worker %6lldms t=%s] MeshtasticWorker::stop() joined cleanly\n", nowMs(), threadTag().c_str());
}

void MeshtasticWorker::setChannels(std::vector<KnownChannel> channels) {
    std::lock_guard<std::mutex> lock(channelsMutex_);
    channels_ = std::move(channels);
    channels_.push_back(makePrimaryChannel());
}

bool MeshtasticWorker::sendText(const std::string& channelName, const std::string& text) {
    if (findChannel(channelName, channelsSnapshot()) == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(outgoingMutex_);
    outgoingQueue_.push_back({channelName, text});
    return true;
}

std::vector<KnownChannel> MeshtasticWorker::channelsSnapshot() {
    std::lock_guard<std::mutex> lock(channelsMutex_);
    return channels_;
}

std::vector<std::string> MeshtasticWorker::channelNames() {
    std::vector<std::string> names;
    for (const auto& channel : channelsSnapshot()) {
        names.push_back(channel.name);
    }
    return names;
}

const KnownChannel* MeshtasticWorker::findChannel(const std::string& name, const std::vector<KnownChannel>& channels) {
    for (const auto& channel : channels) {
        if (channel.name == name) {
            return &channel;
        }
    }
    return nullptr;
}

void MeshtasticWorker::run() {
    using namespace std::chrono_literals;

    cardmesh::lora::RadioParams params;
    params.frequencyMhz = cardmesh::mesh::loraRegionPrimaryFrequencyMhz(settings_.loraRegion);
    params.bandwidthKhz = 250.0f;   // MEDIUM_FAST preset
    params.spreadingFactor = 9;     // MEDIUM_FAST preset
    params.codingRate = 5;          // MEDIUM_FAST preset (4/5)
    params.syncWord = 0x2b;         // Meshtastic's public sync word (RadioLibInterface.h)
    params.outputPowerDbm = 20;     // conservative default; US915 allows up to 30
    params.preambleSymbols = 8;     // non-wideLora default (preambleLengthDefault)
    params.tcxoVoltage = 3.0f;
    params.currentLimitMa = 140.0f;

    const bool ok = radio_.begin(params);
    std::fprintf(stderr, "[worker %6lldms t=%s] MeshtasticWorker::run() radio_.begin() -> %s\n", nowMs(), threadTag().c_str(), ok ? "ok" : radio_.diagnostics().c_str());
    {
        std::lock_guard<std::mutex> lock(messages_.mutex);
        messages_.radioReady = ok;
        messages_.statusMessage = ok ? "LORA: READY" : ("LORA: FAILED - " + radio_.diagnostics());
    }
    if (!ok) {
        running_ = false;
        std::fprintf(stderr, "[worker %6lldms t=%s] MeshtasticWorker::run() exiting (begin failed)\n", nowMs(), threadTag().c_str());
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

void MeshtasticWorker::sendNext() {
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
    const KnownChannel* channel = findChannel(outgoing.channelName, channels);
    if (channel == nullptr) {
        return;
    }

    const std::uint32_t packetId = randomU32();
    auto frame = cardmesh::mesh::encodePacket(settings_.loraNodeNum, PacketHeader::kBroadcast, packetId, *channel,
                                              /*TEXT_MESSAGE_APP=*/1,
                                              reinterpret_cast<const std::uint8_t*>(outgoing.text.data()),
                                              outgoing.text.size());

    const bool sent = radio_.send(frame.data(), frame.size());
    radio_.startReceive();

    if (sent) {
        LoraMessageEntry entry;
        entry.channelName = outgoing.channelName;
        entry.fromNode = settings_.loraNodeNum;
        entry.toNode = PacketHeader::kBroadcast;
        entry.text = outgoing.text;
        entry.timestamp = static_cast<std::int64_t>(std::time(nullptr));
        entry.outgoing = true;

        std::lock_guard<std::mutex> lock(messages_.mutex);
        messages_.messages.push_back(std::move(entry));
    }
}

void MeshtasticWorker::pollIncoming() {
    std::vector<std::uint8_t> packet;
    float rssi = 0.0f;
    float snr = 0.0f;
    if (!radio_.poll(&packet, &rssi, &snr)) {
        return;
    }

    DecodedPacket decoded;
    const auto channels = channelsSnapshot();
    if (!cardmesh::mesh::decodePacket(packet.data(), packet.size(), channels, decoded)) {
        return;  // not from a channel we know, or malformed
    }
    handleDecodedPacket(decoded, rssi, snr);
}

void MeshtasticWorker::handleDecodedPacket(const DecodedPacket& decoded, float rssi, float snr) {
    constexpr std::uint32_t kTextMessageApp = 1;
    constexpr std::uint32_t kPositionApp = 3;

    if (decoded.data.portnum == kTextMessageApp) {
        LoraMessageEntry entry;
        entry.channelName = decoded.channelName;
        entry.fromNode = decoded.header.from;
        entry.toNode = decoded.header.to;
        entry.text.assign(decoded.data.payload.begin(), decoded.data.payload.end());
        entry.timestamp = static_cast<std::int64_t>(std::time(nullptr));
        entry.outgoing = false;
        entry.rssi = rssi;
        entry.snr = snr;

        std::lock_guard<std::mutex> lock(messages_.mutex);
        messages_.messages.push_back(std::move(entry));
    } else if (decoded.data.portnum == kPositionApp) {
        cardmesh::mesh::pb::PositionMessage position;
        if (cardmesh::mesh::pb::decodePosition(decoded.data.payload.data(), decoded.data.payload.size(), position) &&
            position.hasLatLon) {
            char idBuf[16];
            std::snprintf(idBuf, sizeof(idBuf), "!%08x", decoded.header.from);

            MapNodeEntry entry;
            entry.id = idBuf;
            entry.name = idBuf;
            entry.latitude = position.latitude;
            entry.longitude = position.longitude;
            heardPositions_[decoded.header.from] = entry;

            std::vector<MapNodeEntry> current;
            current.reserve(heardPositions_.size());
            for (const auto& [nodeNum, nodeEntry] : heardPositions_) {
                current.push_back(nodeEntry);
            }
            mapModel_.replaceSourceNodes(MapNodeSource::DirectLora, std::move(current));
        }
    }
}

}  // namespace cardmesh::ui
