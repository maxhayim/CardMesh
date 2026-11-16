#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include "DashboardModel.h"
#include "MapModel.h"
#include "models/Channel.h"
#include "storage/Settings.h"

namespace cardmesh::ui {

// Polls MeshMonitor on a background thread and writes results into a
// DashboardModel and MapModel, so the LVGL UI thread never blocks on
// network I/O (see the README's "Background Architecture" section).
class NetworkWorker {
public:
    NetworkWorker(storage::AppSettings settings, DashboardModel& model, MapModel& mapModel);
    ~NetworkWorker();

    void start();
    void stop();
    void requestRefreshNow();

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

    storage::AppSettings settings_;
    DashboardModel& model_;
    MapModel& mapModel_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> refreshRequested_{true};
};

}  // namespace cardmesh::ui
