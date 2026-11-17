#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace cardmesh::ui {

struct MeshCoreMessageEntry {
    std::string senderName;  // decoded from "<senderName>: <text>", or "?" if absent
    std::string text;
    std::int64_t timestamp = 0;
    bool outgoing = false;
    float rssi = 0.0f;
    float snr = 0.0f;
};

// Shared state written by MeshCoreWorker (background thread) and read by
// the UI thread. Kept separate from LoraMessagesModel -- direct MeshCore
// and direct Meshtastic are different protocols over the same radio, never
// merged, and (since only one can own the physical radio at a time) never
// both populated live at once.
struct MeshCoreMessagesModel {
    std::mutex mutex;

    bool radioReady = false;
    std::string statusMessage = "MESHCORE: STARTING";
    std::vector<MeshCoreMessageEntry> messages;
};

}  // namespace cardmesh::ui
