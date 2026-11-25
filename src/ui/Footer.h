#pragma once

#include <array>

#include "lvgl.h"

namespace cardmesh::ui {

constexpr int kFooterItemCount = 5;

// One segment of the footer bar. `badge` is a 1-2 char key label (e.g.
// "F1") -- this hardware has no physical function-key row (see
// EvdevKeyboard.h), so it's a visual hint only, not a real scancode.
// `icon` is an LVGL symbol glyph, or nullptr to show no icon (not every
// action has a built-in glyph that fits). `label` is a short action name.
// An item with label == nullptr renders as an empty, unlabeled segment --
// used when a screen genuinely has fewer than kFooterItemCount real
// actions, rather than inventing a fake one to fill the slot.
struct FooterItem {
    const char* badge = nullptr;
    const char* icon = nullptr;
    const char* label = nullptr;
};

// Reusable bottom navigation bar: rounded mint outline, flat black
// background, kFooterItemCount evenly spaced [badge][icon][label]
// segments. This is the same visual design as the Home screen's original
// single-row footer (now removed from Home itself, see HomeScreen.cpp),
// generalized so any screen can mount one with its own context-sensitive
// items instead of the old hardcoded Home/Radio/Files/Info/Power set.
//
// Presentation only: this hardware has no touch/pointer input (see
// EvdevKeyboard.h's comment on bypassing LVGL's indev/group system), so
// segments are never clicked. Each screen's existing keyboard dispatch in
// ui_main.cpp remains the single source of truth for what each key does;
// setItems() exists purely to keep the on-screen labels in sync with that,
// and callers are expected to pick items that name a binding which is
// actually active for the screen/state it's shown in.
class Footer {
public:
    // Builds the bar as a child of `parent` (a screen's root object, which
    // must have zero padding -- see HomeScreen.cpp for the convention this
    // assumes), anchored to the bottom at the approved margin/height.
    // `highlightedIndex` selects which segment gets the solid accent-fill
    // "primary action" treatment (matches the original design's
    // highlighted first key); pass -1 for no highlight.
    Footer(lv_obj_t* parent, const std::array<FooterItem, kFooterItemCount>& items, int highlightedIndex = 0);

    void setItems(const std::array<FooterItem, kFooterItemCount>& items, int highlightedIndex = 0);

    lv_obj_t* obj() const { return bar_; }

private:
    void rebuild();

    lv_obj_t* bar_ = nullptr;
    std::array<FooterItem, kFooterItemCount> items_{};
    int highlightedIndex_ = 0;
};

}  // namespace cardmesh::ui
