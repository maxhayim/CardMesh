#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace cardmesh::update {

// Shared state written by UpdateWorker (background thread) and read by the
// LVGL UI timer (main thread). All access must hold `mutex` -- same pattern
// as DashboardModel/MapModel.
struct UpdateModel {
    std::mutex mutex;

    bool checking = false;
    bool checked = false;
    bool updateAvailable = false;
    std::string latestVersion;
    std::string downloadUrl;
    std::string statusMessage;  // set after a check or an install attempt
};

// Runs update checks and installs on a background thread so neither blocks
// the UI thread. Mirrors NetworkWorker's run-loop shape but only acts when
// explicitly asked to (checkNow()/installNow()) -- there is no periodic
// polling, since CardMesh is a handheld device expected to run in short
// sessions rather than stay resident (see docs/DEVICE_BUILD.md).
class UpdateWorker {
public:
    explicit UpdateWorker(UpdateModel& model, std::string repo = "maxhayim/CardMesh");
    ~UpdateWorker();

    void start();
    void stop();

    // Requests an update check on the background thread. Safe to call
    // repeatedly; a check already in flight is not duplicated.
    void checkNow();

    // Requests downloading and installing the update found by the last
    // successful checkNow() call. No-op if no update is known to be
    // available.
    void installNow();

private:
    void run();

    UpdateModel& model_;
    std::string repo_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> checkRequested_{false};
    std::atomic<bool> installRequested_{false};
};

}  // namespace cardmesh::update
