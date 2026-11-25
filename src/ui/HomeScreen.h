#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "lvgl.h"

namespace cardmesh::ui {

// Which tile is highlighted/selected on the Home icon grid, in left-to-right
// order (matches the tiles_ array index in HomeScreen.cpp).
enum class HomeTile { Meshtastic = 0, MeshCore = 1, MeshMonitor = 2, Settings = 3 };

constexpr int kHomeTileCount = 4;

// The app's icon-grid landing screen -- three tiles (Meshtastic, MeshCore,
// MeshMonitor), keyboard-navigable left/right with Enter to open one, each
// showing a lit/hollow badge for whether that protocol currently owns the
// radio/connection. See src/ui_main.cpp for the navigation wiring and the
// 'S' key that flips whether this or the Dashboard is home on next launch.
class HomeScreen {
public:
    HomeScreen();

    lv_obj_t* screenObj() const { return screen_; }

    void moveLeft();
    void moveRight();
    HomeTile selected() const { return static_cast<HomeTile>(selectedIndex_); }

    // Cheap no-op diffing like other screens' refresh() -- call every tick
    // while this screen is active.
    void refresh(bool meshtasticActive, bool meshCoreActive, bool meshMonitorActive);

private:
    void build();
    void updateSelectionHighlight();
    void updateStatusRow();

    struct Tile {
        lv_obj_t* card = nullptr;
        // Settings (index 3) has no "owns the radio/connection" concept, so
        // it never gets a badge object at all -- stays null, see build().
        lv_obj_t* badge = nullptr;
        // Only set for tiles with a real logo asset (not Settings, which
        // has no official logo and uses a drawn gear glyph instead). Owns
        // the decoded PNG bytes that lv_image_dsc_t::data points into --
        // must outlive the lv_image object, see build().
        std::unique_ptr<std::vector<std::uint8_t>> logoBytes;
        std::unique_ptr<lv_image_dsc_t> logoDsc;
    };

    lv_obj_t* screen_ = nullptr;
    std::array<Tile, kHomeTileCount> tiles_;
    int selectedIndex_ = 0;
    bool lastActive_[kHomeTileCount] = {false, false, false, false};
    bool everRefreshed_ = false;

    // Top-right status row: real data read from sysfs (battery %, wlan0
    // link state, hci0 rfkill state), not decorative placeholders -- see
    // updateStatusRow() in HomeScreen.cpp. Re-read at most once a second
    // (sysfsPollIntervalMs_ worth of wall-clock time), not every refresh()
    // tick, since refresh() is called roughly every 50ms by ui_main.cpp's
    // main loop and these values change far slower than that.
    lv_obj_t* wifiIcon_ = nullptr;
    lv_obj_t* btIcon_ = nullptr;
    lv_obj_t* batteryIcon_ = nullptr;
    lv_obj_t* statusDivider_ = nullptr;
    lv_obj_t* clockLabel_ = nullptr;
    std::chrono::steady_clock::time_point lastStatusPollAt_{};
};

}  // namespace cardmesh::ui
