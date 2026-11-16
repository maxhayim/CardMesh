#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "DashboardModel.h"
#include "MapModel.h"
#include "MeshMonitorMessagesModel.h"
#include "models/Channel.h"
#include "storage/Settings.h"

namespace cardmesh::ui {

// Polls MeshMonitor on a background thread and writes results into a
// DashboardModel, MapModel, and MeshMonitorMessagesModel, so the LVGL UI
// thread never blocks on network I/O (see the README's "Background
// Architecture" section).
class NetworkWorker {
public:
    NetworkWorker(storage::AppSettings settings, DashboardModel& model, MapModel& mapModel,
                  MeshMonitorMessagesModel& messagesModel);
    ~NetworkWorker();

    void start();
    void stop();
    void requestRefreshNow();

    // Queues an outgoing message on the current primary channel, sent from
    // the background thread on its next poll. No-op (returns false) until
    // the first successful poll has established a primary channel id.
    bool queueSendMessage(const std::string& text);

    // Called from the background thread after each successful channel
    // fetch, with MeshMonitor's channel list (including each channel's
    // base64 PSK, where exposed). Lets the owner feed real channel keys
    // into MeshtasticWorker for direct-LoRa decoding, without NetworkWorker
    // needing to know that type exists. Set before start(); not
    // thread-safe to change afterward.
    std::function<void(const std::vector<models::Channel>&)> onChannelsUpdated;

private:
    void run();
    void pollOnce();
    void sendQueuedMessages(const std::string& sourceId, const std::string& channelId);

    storage::AppSettings settings_;
    DashboardModel& model_;
    MapModel& mapModel_;
    MeshMonitorMessagesModel& messagesModel_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> refreshRequested_{true};

    // Worker-thread-only, not shared directly: dedupes getMessages()'s
    // (likely non-incremental) response against what's already in
    // messagesModel_.
    std::set<std::string> seenMessageIds_;

    std::mutex outgoingMutex_;
    std::deque<std::string> outgoingQueue_;
};

}  // namespace cardmesh::ui
