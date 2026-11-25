#include "HomeScreen.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "Theme.h"
#include "hardware/HardwareStatus.h"

namespace cardmesh::ui {

using cardmesh::hardware::BatteryStatus;
using cardmesh::hardware::BluetoothState;
using cardmesh::hardware::WifiState;
using cardmesh::hardware::readBatteryStatus;
using cardmesh::hardware::readBluetoothState;
using cardmesh::hardware::readWifiSignalPercent;
using cardmesh::hardware::readWifiState;

namespace {

// Meshtastic, MeshCore and MeshMonitor each ship a real logo, pre-converted
// (see scratchpad's logo-recolor scripts, not checked in) from the
// original brand artwork to unified monochrome mint-green (#8FE6A5) on a
// transparent background, as a 56x56 PNG under assets/branding/logos/ --
// preserves the real mountain/wordmark/node-graph geometry per explicit
// direction, rather than reconstructing it from generic LVGL primitives.
// Settings has no official logo and draws a plain gear glyph instead.

std::filesystem::path logoDir() {
    const char* overrideDir = std::getenv("CARDMESH_LOGO_DIR");
    // Production default is an absolute path under the install tree (see
    // scripts/package_deb.py), not a path relative to the process's cwd,
    // which isn't guaranteed to be anything in particular on-device.
    return overrideDir != nullptr ? overrideDir : "/usr/lib/cardmesh/assets/branding/logos";
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// Loads a logo PNG and scales it from its native 56x56 down to boxSize,
// matching the MapScreen tile-loading pattern: LV_COLOR_FORMAT_RAW with
// w/h/stride left at 0 lets lv_lodepng auto-sniff the real PNG dimensions
// at decode time, and bytesOut/dscOut must outlive the lv_image object
// since dscOut->data points directly into bytesOut's buffer.
//
// extraZoom256 is a second zoom multiplier (256 == 1.0x) applied on top of
// the boxSize fit, for logos whose actual artwork doesn't fill their own
// 56x56 canvas the same amount as the others do -- measured opaque-pixel
// bounding boxes: Meshtastic 48x26, MeshCore 54x54 (near full-bleed, since
// it's the real badge+lettering), MeshMonitor 43x30. Without this, equal
// *container* sizes still produce unequal *visual* sizes (MeshCore reading
// much larger/more prominent than the other two). The surrounding
// transparent padding that grows past boxSize when extraZoom256 != 256 is
// harmless (nothing opaque paints there), so no extra clipping/centering
// logic is needed.
void createLogoImage(lv_obj_t* parent, const char* filename, int boxSize, int extraZoom256,
                      std::unique_ptr<std::vector<std::uint8_t>>& bytesOut, std::unique_ptr<lv_image_dsc_t>& dscOut) {
    bytesOut = std::make_unique<std::vector<std::uint8_t>>(readFile(logoDir() / filename));

    dscOut = std::make_unique<lv_image_dsc_t>();
    dscOut->header.magic = LV_IMAGE_HEADER_MAGIC;
    dscOut->header.cf = LV_COLOR_FORMAT_RAW;
    dscOut->header.flags = 0;
    dscOut->header.w = 0;
    dscOut->header.h = 0;
    dscOut->header.stride = 0;
    dscOut->header.reserved_2 = 0;
    dscOut->data_size = static_cast<std::uint32_t>(bytesOut->size());
    dscOut->data = bytesOut->data();
    dscOut->reserved = nullptr;

    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, dscOut.get());
    constexpr int kSourceLogoPx = 56;
    const int baseZoom = (256 * boxSize) / kSourceLogoPx;
    lv_image_set_scale(img, (baseZoom * extraZoom256) / 256);
    lv_obj_center(img);
}

// LVGL's built-in LV_SYMBOL_SETTINGS glyph renders as a solid filled gear,
// not the outlined one in the reference -- there's no outline variant in
// the compiled font, so this is hand-drawn instead: a 6-tooth gear
// silhouette (flat-topped teeth, valley-to-valley approximated as straight
// chords rather than arcs, negligible at this size) as a closed stroked
// polyline, plus a separate stroked circle for the center hole. Both
// outline-only (transparent interior), matching the reference's stroke
// style, not a filled shape. Tooth count (6, not 8) and proportions were
// counted/measured directly off the reference image, then scaled by 1.4x
// (coordinates and hole size/position only, not stroke width) to match the
// Home card icon box growing from 40x40 to 56x56 -- still centered within
// the new box (drawing spans x/y 8..48 inside a 56-wide box: 8px margin on
// both sides).
void drawSettingsIcon(lv_obj_t* parent) {
    static constexpr lv_point_precise_t kGearOutline[] = {
        {24, 14}, {22, 8},  {34, 8},  {32, 14}, {38, 18}, {43, 13}, {48, 22}, {42, 24},
        {42, 32}, {48, 34}, {43, 43}, {38, 38}, {32, 42}, {34, 48}, {22, 48}, {24, 42},
        {18, 38}, {13, 43}, {8, 34},  {14, 32}, {14, 24}, {8, 22},  {13, 13}, {18, 18},
        {24, 14},  // repeated first point to close the loop
    };

    lv_obj_t* gear = lv_line_create(parent);
    lv_line_set_points(gear, kGearOutline, sizeof(kGearOutline) / sizeof(kGearOutline[0]));
    lv_obj_set_style_line_width(gear, 2, 0);
    lv_obj_set_style_line_color(gear, theme::accent(), 0);
    lv_obj_set_style_line_rounded(gear, true, 0);
    lv_obj_set_pos(gear, 0, 0);

    lv_obj_t* hole = lv_obj_create(parent);
    lv_obj_set_size(hole, 15, 15);
    lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(hole, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hole, 2, 0);
    lv_obj_set_style_border_color(hole, theme::accent(), 0);
    lv_obj_set_pos(hole, 21, 21);
}

struct TileSpec {
    const char* name;
    // nullptr => no official logo exists; drawIcon is used instead
    // (Settings only, see drawSettingsIcon).
    const char* logoFile;
    // See createLogoImage()'s extraZoom256 parameter -- 256 means no extra
    // scaling beyond the normal boxSize fit.
    int extraZoom256;
    void (*drawIcon)(lv_obj_t* parent);
};

constexpr TileSpec kTileSpecs[kHomeTileCount] = {
    {"Meshtastic", "meshtastic.png", 256, nullptr},
    // MeshCore's real badge art is near full-bleed within its 56x56 canvas
    // (54x54 opaque bbox) vs. Meshtastic's 48x26 -- scaled down slightly so
    // it doesn't read larger/heavier than the other two tiles.
    {"MeshCore", "meshcore.png", 225, nullptr},
    // MeshMonitor's opaque content is only 43x30 of its 56x56 canvas --
    // scaled up to match Meshtastic's visual footprint.
    {"MeshMonitor", "meshmonitor.png", 290, nullptr},
    {"Settings", nullptr, 256, &drawSettingsIcon},
};

// A single consistent left/right margin for every row -- title, status
// icons, divider, tile grid, and hint bar -- per the approved design spec's
// "minimal padding" header requirement.
constexpr int kMargin = 7;

}  // namespace

HomeScreen::HomeScreen() { build(); }

void HomeScreen::build() {
    screen_ = lv_obj_create(nullptr);
    lv_obj_t* screen = screen_;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    // Two-tone wordmark ("Card" white, "Mesh" accent green) as two adjacent
    // labels rather than one recolored string -- simpler than enabling
    // LVGL's inline-recolor markup for a static two-segment title.
    lv_obj_t* titleCard = lv_label_create(screen);
    lv_label_set_text(titleCard, "Card");
    lv_obj_set_style_text_font(titleCard, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(titleCard, lv_color_white(), 0);
    lv_obj_align(titleCard, LV_ALIGN_TOP_LEFT, kMargin, 0);

    lv_obj_t* titleMesh = lv_label_create(screen);
    lv_label_set_text(titleMesh, "Mesh");
    lv_obj_set_style_text_font(titleMesh, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(titleMesh, theme::accent(), 0);
    lv_obj_align_to(titleMesh, titleCard, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    // Top-right status row: real battery/WiFi/Bluetooth/clock. Only created
    // here; updateStatusRow() (called once below, then from refresh() on
    // its own poll interval) sets their content AND redoes the right-to-
    // left alignment chain every time, since lv_obj_align_to is a one-shot
    // position, not a live constraint -- it must be reapplied whenever a
    // label's own width can change (the clock's "9:05" vs "13:00", or the
    // battery glyph switching levels).
    clockLabel_ = lv_label_create(screen);
    lv_obj_set_style_text_color(clockLabel_, lv_color_white(), 0);

    statusDivider_ = lv_label_create(screen);
    lv_label_set_text(statusDivider_, "|");
    lv_obj_set_style_text_color(statusDivider_, theme::accent(), 0);

    batteryIcon_ = lv_label_create(screen);

    btIcon_ = lv_label_create(screen);
    lv_label_set_text(btIcon_, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(btIcon_, lv_color_white(), 0);

    wifiIcon_ = lv_label_create(screen);
    lv_label_set_text(wifiIcon_, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifiIcon_, theme::accent(), 0);

    updateStatusRow();

    // Header divider sits right under the title/status row, not measured
    // off any mockup fraction -- just far enough down to clear both.
    theme::makeDivider(screen, 320 - 2 * kMargin, kMargin, 22);

    // v32 nav redesign: the footer that used to sit below these cards
    // moved into service screens as a reusable, context-sensitive
    // component (see Footer.h) -- Home no longer has one, so the cards
    // grow to fill the reclaimed vertical space. margin=12, gap=12,
    // width=65 -- 4*65 + 3*12 + 2*12 == 320 exactly, same approach as the
    // original v31 measurement, just re-solved for the new taller cards.
    // Logos render at their native 56x56 (kLogoBoxSize), so the common
    // case (extraZoom256 == 256, see createLogoImage) is pixel-crisp, not
    // upscaled.
    constexpr int kCardMargin = 12;
    constexpr int kCardGap = 12;
    constexpr int kCardWidth = 65;
    constexpr int kCardHeight = 120;
    constexpr int kCardTopY = 36;
    constexpr int kLogoBoxSize = 56;
    // Logo and label are centered as a flex column (not fixed top/bottom
    // offsets) so they read as one balanced group regardless of card
    // height, instead of the logo pinned high and the label pinned low
    // with a large dead gap between them.
    constexpr int kCardContentGap = 10;

    for (int i = 0; i < kHomeTileCount; i++) {
        const int cardX = kCardMargin + i * (kCardWidth + kCardGap);
        lv_obj_t* card = lv_obj_create(screen);
        lv_obj_set_size(card, kCardWidth, kCardHeight);
        lv_obj_set_pos(card, cardX, kCardTopY);
        theme::applyCardStyle(card);
        // Selection is conveyed by a stronger border (3px vs 1px) AND a
        // subtle dark mint-tinted fill on the selected card only --
        // unselected cards stay pure black/transparent, matching the
        // approved design's flat look (no glow/shadow, just a tinted
        // fill). updateSelectionHighlight() keeps both of these in sync
        // with selectedIndex_ as it changes; this is just the initial
        // state.
        //
        // Opacity kept very low (LV_OPA_20, not LV_OPA_COVER): direct pixel
        // sampling of the reference image's own selected card shows a pure
        // (0,0,0) interior, identical to the unselected cards -- no fill at
        // all, and a border profile pixel-for-pixel the same thickness as
        // the unselected cards'. A faint tint is kept anyway rather than
        // zero, since keyboard navigation needs *some* visible affordance
        // for which card is focused.
        const bool initiallySelected = (i == 0);
        lv_obj_set_style_bg_color(card, theme::accentDim(), 0);
        lv_obj_set_style_bg_opa(card, initiallySelected ? LV_OPA_20 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(card, initiallySelected ? 2 : 1, 0);
        lv_obj_set_style_border_color(card, theme::accent(), 0);
        // Without this, lv_obj's default top padding (~16px) silently
        // shifted every TOP-aligned child down by that much -- found by
        // bisecting why a label aligned well within the card's bounds was
        // rendering clipped off almost entirely near the card's bottom edge.
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(card, kCardContentGap, 0);

        lv_obj_t* iconBox = lv_obj_create(card);
        lv_obj_set_size(iconBox, kLogoBoxSize, kLogoBoxSize);
        lv_obj_set_style_bg_opa(iconBox, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(iconBox, 0, 0);
        lv_obj_set_style_pad_all(iconBox, 0, 0);
        lv_obj_clear_flag(iconBox, LV_OBJ_FLAG_SCROLLABLE);
        if (kTileSpecs[i].logoFile != nullptr) {
            createLogoImage(iconBox, kTileSpecs[i].logoFile, kLogoBoxSize, kTileSpecs[i].extraZoom256,
                             tiles_[i].logoBytes, tiles_[i].logoDsc);
        } else {
            kTileSpecs[i].drawIcon(iconBox);
        }

        lv_obj_t* label = lv_label_create(card);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_letter_space(label, -1, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(label, kTileSpecs[i].name);
        lv_obj_set_width(label, kCardWidth - 2);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);

        tiles_[i].card = card;

        // Settings has no "owns the radio/connection" concept, so it gets
        // no status dot at all (not even a hollow one) -- matches the
        // approved design, where only the three protocol tiles have one.
        // FLOATING excludes it from the card's flex column (an absolute
        // corner overlay, not a third flow item sharing space with the
        // logo/label).
        if (i < 3) {
            lv_obj_t* badge = lv_obj_create(card);
            lv_obj_set_size(badge, 8, 8);
            lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(badge, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(badge, 1, 0);
            lv_obj_set_style_border_color(badge, theme::accent(), 0);
            lv_obj_add_flag(badge, LV_OBJ_FLAG_FLOATING);
            lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -4, 4);
            tiles_[i].badge = badge;
        }
    }

    // v32: no footer here anymore -- Home is keyboard-navigated directly
    // (PanLeft/PanRight/StartCompose, see ui_main.cpp) and doesn't need a
    // key-hint bar of its own. See Footer.h for where this moved.
    updateSelectionHighlight();
}

void HomeScreen::updateStatusRow() {
    char clockBuf[8];
    const std::time_t now = std::time(nullptr);
    std::tm localNow{};
    localtime_r(&now, &localNow);
    std::strftime(clockBuf, sizeof(clockBuf), "%H:%M", &localNow);
    lv_label_set_text(clockLabel_, clockBuf);

    const BatteryStatus battery = readBatteryStatus();
    const char* batteryGlyph = LV_SYMBOL_BATTERY_EMPTY;
    lv_color_t batteryColor = theme::accent();
    if (battery.percent < 0) {
        // Unavailable (no battery sysfs entry found) -- dim(), not accent(),
        // so this can't be mistaken for a healthy/full battery at a glance.
        batteryGlyph = LV_SYMBOL_BATTERY_EMPTY;
        batteryColor = theme::dim();
    } else if (battery.chargingKnown && battery.charging) {
        batteryGlyph = LV_SYMBOL_CHARGE;
        batteryColor = theme::accent();
    } else if (battery.percent >= 80) {
        batteryGlyph = LV_SYMBOL_BATTERY_FULL;
    } else if (battery.percent >= 55) {
        batteryGlyph = LV_SYMBOL_BATTERY_3;
    } else if (battery.percent >= 30) {
        batteryGlyph = LV_SYMBOL_BATTERY_2;
    } else if (battery.percent >= 15) {
        batteryGlyph = LV_SYMBOL_BATTERY_1;
    } else {
        batteryGlyph = LV_SYMBOL_BATTERY_EMPTY;
        batteryColor = theme::warn();
    }
    lv_label_set_text(batteryIcon_, batteryGlyph);
    lv_obj_set_style_text_color(batteryIcon_, batteryColor, 0);

    // WiFi: accent() + (if available) actual signal percent when connected,
    // matching the approved v31 look exactly for the common case. dim() at
    // reduced opacity distinguishes "hardware present but not connected"
    // from "no wifi hardware found at all" -- previously indistinguishable
    // since the read result was discarded entirely.
    const WifiState wifiState = readWifiState();
    std::string wifiText = LV_SYMBOL_WIFI;
    lv_color_t wifiColor = theme::dim();
    lv_opa_t wifiOpa = LV_OPA_COVER;
    if (wifiState == WifiState::Connected) {
        wifiColor = theme::accent();
        const int signal = readWifiSignalPercent();
        if (signal >= 0) {
            wifiText += " " + std::to_string(signal) + "%";
        }
    } else if (wifiState == WifiState::Disconnected) {
        wifiColor = theme::dim();
    } else {
        wifiColor = theme::dim();
        wifiOpa = LV_OPA_50;
    }
    lv_label_set_text(wifiIcon_, wifiText.c_str());
    lv_obj_set_style_text_color(wifiIcon_, wifiColor, 0);
    lv_obj_set_style_text_opa(wifiIcon_, wifiOpa, 0);

    // Bluetooth: white when enabled, matching the approved v31 look exactly
    // for the common case; dim() for disabled/unavailable. This is adapter
    // state only (rfkill), not a Meshtastic/MeshCore connection -- see
    // readBluetoothState()'s comment for why an actual paired-connection
    // indicator isn't implemented here.
    const BluetoothState btState = readBluetoothState();
    lv_color_t btColor = lv_color_white();
    lv_opa_t btOpa = LV_OPA_COVER;
    if (btState == BluetoothState::Disabled) {
        btColor = theme::dim();
    } else if (btState == BluetoothState::Unavailable) {
        btColor = theme::dim();
        btOpa = LV_OPA_50;
    }
    lv_obj_set_style_text_color(btIcon_, btColor, 0);
    lv_obj_set_style_text_opa(btIcon_, btOpa, 0);

    // Re-chain right-to-left: lv_obj_align_to is a one-shot position, not a
    // live constraint, and every label above may have just changed width
    // (the clock's "9:05" vs "13:00", the battery glyph switching levels,
    // the wifi label gaining/losing a signal-percent suffix).
    lv_obj_align(clockLabel_, LV_ALIGN_TOP_RIGHT, -kMargin, 4);
    lv_obj_align_to(statusDivider_, clockLabel_, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    lv_obj_align_to(batteryIcon_, statusDivider_, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    lv_obj_align_to(btIcon_, batteryIcon_, LV_ALIGN_OUT_LEFT_MID, -8, 0);
    lv_obj_align_to(wifiIcon_, btIcon_, LV_ALIGN_OUT_LEFT_MID, -8, 0);
}

void HomeScreen::updateSelectionHighlight() {
    for (int i = 0; i < kHomeTileCount; i++) {
        const bool isSelected = (i == selectedIndex_);
        lv_obj_set_style_border_width(tiles_[i].card, isSelected ? 2 : 1, 0);
        lv_obj_set_style_bg_opa(tiles_[i].card, isSelected ? LV_OPA_20 : LV_OPA_TRANSP, 0);
    }
}

void HomeScreen::moveLeft() {
    selectedIndex_ = (selectedIndex_ + kHomeTileCount - 1) % kHomeTileCount;
    updateSelectionHighlight();
}

void HomeScreen::moveRight() {
    selectedIndex_ = (selectedIndex_ + 1) % kHomeTileCount;
    updateSelectionHighlight();
}

void HomeScreen::refresh(bool meshtasticActive, bool meshCoreActive, bool meshMonitorActive) {
    // ui_main.cpp calls refresh() roughly every 50ms; the status row's
    // sysfs reads only need to happen about once a second, matching how
    // fast the clock/battery/radio state actually change.
    const auto now = std::chrono::steady_clock::now();
    if (now - lastStatusPollAt_ >= std::chrono::seconds(1)) {
        updateStatusRow();
        lastStatusPollAt_ = now;
    }

    // Settings has no badge (index 3) -- only the first 3 are touched.
    const bool active[3] = {meshtasticActive, meshCoreActive, meshMonitorActive};
    for (int i = 0; i < 3; i++) {
        if (everRefreshed_ && active[i] == lastActive_[i]) {
            continue;
        }
        lv_obj_set_style_bg_opa(tiles_[i].badge, active[i] ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(tiles_[i].badge, theme::accent(), 0);
        lastActive_[i] = active[i];
    }
    everRefreshed_ = true;
}

}  // namespace cardmesh::ui
