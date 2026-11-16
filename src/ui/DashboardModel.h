#pragma once

#include <mutex>
#include <string>

namespace cardmesh::ui {

// Shared state written by NetworkWorker (background thread) and read by the
// LVGL UI timer (main thread). All access must hold `mutex`.
struct DashboardModel {
    std::mutex mutex;

    bool connected = false;
    std::string statusMessage = "CONNECTING...";

    std::string sourceName;
    int sourceCount = 0;
    int nodeCount = 0;
    int activeCount = 0;
    int channelCount = 0;
    int unreadCount = 0;

    // Mirrors cardmesh::update::UpdateModel's fields, duplicated here so
    // DashboardScreen only has to read one model. Written by ui_main.cpp
    // after reading the real UpdateModel (see src/update/UpdateWorker.h).
    std::string updateStatusLine;
};

}  // namespace cardmesh::ui
