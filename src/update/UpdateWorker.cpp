#include "UpdateWorker.h"

#include <chrono>
#include <utility>

#include "UpdateChecker.h"
#include "UpdateInstaller.h"
#include "Version.h"

namespace cardmesh::update {

UpdateWorker::UpdateWorker(UpdateModel& model, std::string repo) : model_(model), repo_(std::move(repo)) {}

UpdateWorker::~UpdateWorker() { stop(); }

void UpdateWorker::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&UpdateWorker::run, this);
}

void UpdateWorker::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void UpdateWorker::checkNow() { checkRequested_ = true; }

void UpdateWorker::installNow() { installRequested_ = true; }

void UpdateWorker::run() {
    using namespace std::chrono_literals;

    while (running_) {
        if (checkRequested_.exchange(false)) {
            {
                std::lock_guard<std::mutex> lock(model_.mutex);
                model_.checking = true;
            }

            const auto result = checkForUpdate(kVersion, repo_);

            std::lock_guard<std::mutex> lock(model_.mutex);
            model_.checking = false;
            model_.checked = result.ok;
            model_.updateAvailable = result.updateAvailable;
            model_.latestVersion = result.latestVersion;
            model_.downloadUrl = result.downloadUrl;
            model_.statusMessage = result.ok
                ? (result.updateAvailable ? ("Update available: v" + result.latestVersion) : "Up to date")
                : ("Update check failed: " + result.errorMessage);
        }

        if (installRequested_.exchange(false)) {
            std::string downloadUrl;
            bool available = false;
            {
                std::lock_guard<std::mutex> lock(model_.mutex);
                available = model_.updateAvailable;
                downloadUrl = model_.downloadUrl;
            }

            if (available && !downloadUrl.empty()) {
                const auto install = downloadAndInstall(downloadUrl);

                std::lock_guard<std::mutex> lock(model_.mutex);
                model_.statusMessage = install.message;
                if (install.ok) {
                    model_.updateAvailable = false;
                }
            }
        }

        std::this_thread::sleep_for(100ms);
    }
}

}  // namespace cardmesh::update
