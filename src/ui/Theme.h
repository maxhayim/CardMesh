#pragma once

#include <cstdint>
#include <string>

#include "lvgl.h"

namespace cardmesh::ui::theme {

// Inspired by the official Meshtastic and MeshCore Android apps (not the
// other CardputerZero store apps): a dark background with a soft sage-green
// brand accent -- Meshtastic's own signature color -- rounded charcoal
// "cards" instead of flat dividers, and a red badge convention for unread
// counts, borrowed from MeshCore's contact-list badges. Deliberately not a
// per-sender rainbow palette (that's another store app's specific trick,
// not something either official app does); senders are distinguished by
// plain dim-gray id + white message text, same as both official apps.
inline lv_color_t accent() { return lv_color_make(0x67, 0xea, 0x94); }
inline lv_color_t accentDim() { return lv_color_make(0x2f, 0x5c, 0x47); }
inline lv_color_t card() { return lv_color_make(0x1c, 0x22, 0x1f); }
inline lv_color_t dim() { return lv_color_make(0x8a, 0x95, 0x8e); }
inline lv_color_t warn() { return lv_color_make(0xff, 0xcc, 0x00); }
inline lv_color_t danger() { return lv_color_make(0xe5, 0x5d, 0x5d); }

constexpr int kCardRadius = 8;
constexpr int kDividerRadius = 2;

// A friendlier rounded accent divider in place of a flat 1px white bar.
inline lv_obj_t* makeDivider(lv_obj_t* parent, int width, int x, int y) {
    lv_obj_t* divider = lv_obj_create(parent);
    lv_obj_set_size(divider, width, 2);
    lv_obj_set_style_bg_color(divider, accent(), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_radius(divider, kDividerRadius, 0);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, x, y);
    return divider;
}

// Applies the rounded dark "card" look (Meshtastic's node-list row style)
// to a container -- a slightly lighter charcoal than the screen's pure
// black, with soft rounded corners.
inline void applyCardStyle(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, card(), 0);
    lv_obj_set_style_radius(obj, kCardRadius, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
}

}  // namespace cardmesh::ui::theme
