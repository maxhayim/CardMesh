#include "HomeScreen.h"

#include <cstdio>
#include <fstream>

#include "Theme.h"

namespace cardmesh::ui {

namespace {

struct TileSpec {
    const char* glyph;  // fallback, used whenever logoPath is null or unreadable
    const char* name;
    const char* logoPath;
};

// Meshtastic's and MeshMonitor's own real app icons (see
// assets/branding/logos/ -- sourced from their official GitHub repos,
// github.com/meshtastic/design and github.com/Yeraze/meshmonitor, and
// installed to the paths below by scripts/package_deb.py). MeshCore has no
// official SQUARE icon published anywhere -- only a wordmark logo exists
// in meshcore-dev/MeshCore and in Liam Cottle's own repos -- so its tile
// falls back to a plain glyph rather than stretching/cropping something
// that isn't actually their icon.
constexpr TileSpec kTileSpecs[kHomeTileCount] = {
    {LV_SYMBOL_WIFI, "Meshtastic", "/usr/share/cardmesh/logos/meshtastic.png"},
    {LV_SYMBOL_BLUETOOTH, "MeshCore", nullptr},
    {LV_SYMBOL_LIST, "MeshMonitor", "/usr/share/cardmesh/logos/meshmonitor.png"},
    {LV_SYMBOL_SETTINGS, "Settings", nullptr},
};

std::unique_ptr<std::vector<std::uint8_t>> readFile(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return nullptr;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return nullptr;
    }
    file.seekg(0, std::ios::beg);
    auto bytes = std::make_unique<std::vector<std::uint8_t>>(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes->data()), size)) {
        return nullptr;
    }
    return bytes;
}

}  // namespace

HomeScreen::HomeScreen() { build(); }

void HomeScreen::build() {
    screen_ = lv_obj_create(nullptr);
    lv_obj_t* screen = screen_;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 4, 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "CardMesh");
    lv_obj_set_style_text_color(title, theme::accent(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 2);

    theme::makeDivider(screen, 312, 2, 20);

    constexpr int kTileWidth = 72;
    constexpr int kTileHeight = 100;
    constexpr int kGap = 8;
    constexpr int kTopY = 30;

    for (int i = 0; i < kHomeTileCount; i++) {
        lv_obj_t* card = lv_obj_create(screen);
        lv_obj_set_size(card, kTileWidth, kTileHeight);
        lv_obj_set_pos(card, 2 + i * (kTileWidth + kGap), kTopY);
        theme::applyCardStyle(card);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, theme::card(), 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        auto logoBytes = readFile(kTileSpecs[i].logoPath);
        if (logoBytes != nullptr) {
            auto dsc = std::make_unique<lv_image_dsc_t>();
            dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
            dsc->header.cf = LV_COLOR_FORMAT_RAW;
            dsc->header.flags = 0;
            dsc->header.w = 0;
            dsc->header.h = 0;
            dsc->header.stride = 0;
            dsc->header.reserved_2 = 0;
            dsc->data_size = static_cast<uint32_t>(logoBytes->size());
            dsc->data = logoBytes->data();
            dsc->reserved = nullptr;

            lv_obj_t* logo = lv_image_create(card);
            lv_image_set_src(logo, dsc.get());
            lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 12);

            tiles_[i].logoBytes = std::move(logoBytes);
            tiles_[i].logoDsc = std::move(dsc);
        } else {
            lv_obj_t* glyph = lv_label_create(card);
            lv_label_set_text(glyph, kTileSpecs[i].glyph);
            lv_obj_set_style_text_font(glyph, &lv_font_montserrat_24, 0);
            lv_obj_set_style_text_color(glyph, theme::dim(), 0);
            lv_obj_align(glyph, LV_ALIGN_TOP_MID, 0, 16);
        }

        lv_obj_t* label = lv_label_create(card);
        lv_label_set_text(label, kTileSpecs[i].name);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        // "MeshMonitor" is 11 characters -- too wide for a 72px tile even at
        // this font. LONG_DOT truncates with an ellipsis instead of
        // overflowing into the neighboring tile.
        lv_obj_set_width(label, kTileWidth - 4);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -8);

        // A hollow ring by default; refresh() fills it solid in the brand
        // accent when this protocol currently owns the radio/connection --
        // MeshCore's own contact-list badge convention, repurposed here as
        // an at-a-glance "what's live right now" indicator.
        lv_obj_t* badge = lv_obj_create(card);
        lv_obj_set_size(badge, 10, 10);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(badge, 1, 0);
        lv_obj_set_style_border_color(badge, theme::dim(), 0);
        lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -4, 4);

        tiles_[i].card = card;
        tiles_[i].badge = badge;
    }

    hintLabel_ = lv_label_create(screen);
    lv_label_set_text(hintLabel_, "[Arrows] select  [Enter] open  [S]ettings");
    lv_obj_set_style_text_color(hintLabel_, theme::dim(), 0);
    lv_obj_align(hintLabel_, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    updateSelectionHighlight();
}

void HomeScreen::updateSelectionHighlight() {
    for (int i = 0; i < kHomeTileCount; i++) {
        const bool isSelected = (i == selectedIndex_);
        lv_obj_set_style_border_color(tiles_[i].card, isSelected ? theme::accent() : theme::card(), 0);
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
    // Settings has no "owns the radio/connection" concept, so its badge
    // (index 3) is left hollow forever -- only the first 3 are touched.
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
