#pragma once

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "LoraMessagesModel.h"
#include "MapModel.h"
#include "lora/LoraRadio.h"
#include "mesh/MeshPacketCodec.h"
#include "storage/Settings.h"

namespace cardmesh::ui {

// Drives CardMesh's direct-LoRa path on a background thread: brings up the
// SX1262 hat, listens for and decrypts/decodes real Meshtastic packets on
// known channels, and sends outgoing text messages. Fully independent of
// NetworkWorker/MeshMonitor -- both mesh paths run concurrently, by design
// (see the project's "both meshes active at the same time" requirement).
// Only the map is shared (see MapModel::replaceSourceNodes); messages stay
// in this worker's own LoraMessagesModel.
class MeshtasticWorker {
public:
    // `settings` is read for/written with loraNodeNum (generated once on
    // first start() if still 0, then persisted via storage::Settings::save
    // so it's stable across runs) -- the caller owns its lifetime and must
    // keep it alive for as long as this worker exists.
    MeshtasticWorker(storage::AppSettings& settings, LoraMessagesModel& messages, MapModel& mapModel);
    ~MeshtasticWorker();

    MeshtasticWorker(const MeshtasticWorker&) = delete;
    MeshtasticWorker& operator=(const MeshtasticWorker&) = delete;

    void start();
    void stop();

    // Replaces the set of channels this node will decrypt/decode traffic
    // for and can send on. Always includes the public MediumFast/default-PSK
    // channel in addition to whatever is passed here, so general mesh
    // traffic is visible even with no real channel credentials configured.
    void setChannels(std::vector<mesh::KnownChannel> channels);

    // Queues an outgoing text message; sent from the worker thread on its
    // next idle tick. Returns false only if the named channel isn't known.
    bool sendText(const std::string& channelName, const std::string& text);

private:
    void run();
    void sendNext();
    void pollIncoming();
    void handleDecodedPacket(const mesh::DecodedPacket& decoded, float rssi, float snr);

    std::vector<mesh::KnownChannel> channelsSnapshot();
    const mesh::KnownChannel* findChannel(const std::string& name, const std::vector<mesh::KnownChannel>& channels);

    lora::LoraRadio radio_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    storage::AppSettings& settings_;

    std::mutex channelsMutex_;
    std::vector<mesh::KnownChannel> channels_;

    struct OutgoingMessage {
        std::string channelName;
        std::string text;
    };
    std::mutex outgoingMutex_;
    std::deque<OutgoingMessage> outgoingQueue_;

    std::map<std::uint32_t, MapNodeEntry> heardPositions_;  // worker-thread-only, not shared directly

    LoraMessagesModel& messages_;
    MapModel& mapModel_;
};

}  // namespace cardmesh::ui
