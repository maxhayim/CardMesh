#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
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

    struct Tile {
        lv_obj_t* card = nullptr;
        lv_obj_t* badge = nullptr;
        // Only set for tiles with a real logo image (Meshtastic/MeshMonitor);
        // kept alive here since lv_image_dsc_t only stores a raw pointer
        // into it. MeshCore has no official square icon anywhere public
        // (only a wordmark) -- see HomeScreen.cpp -- so it stays a plain
        // glyph instead.
        std::unique_ptr<std::vector<std::uint8_t>> logoBytes;
        std::unique_ptr<lv_image_dsc_t> logoDsc;
    };

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* hintLabel_ = nullptr;
    std::array<Tile, kHomeTileCount> tiles_;
    int selectedIndex_ = 0;
    bool lastActive_[kHomeTileCount] = {false, false, false, false};
    bool everRefreshed_ = false;
};

}  // namespace cardmesh::ui
