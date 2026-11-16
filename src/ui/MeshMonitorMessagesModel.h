#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "models/Message.h"

namespace cardmesh::ui {

// Shared state written by NetworkWorker (background thread) and read by
// the UI thread. Kept separate from CardMesh's direct-LoRa message list
// (see LoraMessagesModel) -- messages relayed through the MeshMonitor
// server are a distinct source from ones seen directly over the LoRa hat,
// by design (the two mesh paths are not merged).
struct MeshMonitorMessagesModel {
    std::mutex mutex;

    std::vector<models::Message> messages;

    // The channel id NetworkWorker last saw as this source's primary
    // channel -- the send target, since there's no channel picker yet.
    // Empty until the first successful poll.
    std::string primaryChannelId;
};

}  // namespace cardmesh::ui
