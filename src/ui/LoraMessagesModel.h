#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace cardmesh::ui {

struct LoraMessageEntry {
    std::string channelName;
    std::uint32_t fromNode = 0;
    std::uint32_t toNode = 0;
    std::string text;
    std::int64_t timestamp = 0;
    bool outgoing = false;
    float rssi = 0.0f;
    float snr = 0.0f;
};

// Shared state written by MeshtasticWorker (background thread) and read by
// the UI thread. Kept separate from MeshMonitor's own message list (see
// DashboardModel) -- messages seen directly over the LoRa hat are a
// distinct source from ones relayed through the MeshMonitor server, by
// design (the two mesh paths are not merged).
struct LoraMessagesModel {
    std::mutex mutex;

    bool radioReady = false;
    std::string statusMessage = "LORA: STARTING";
    std::vector<LoraMessageEntry> messages;
};

}  // namespace cardmesh::ui
