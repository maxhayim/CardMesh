#include "Footer.h"

#include "Theme.h"

namespace cardmesh::ui {

namespace {

// Same geometry as the Home screen's original footer: a bar inset 6px from
// the screen edges, 28px tall, sitting flush with the bottom margin.
constexpr int kFooterMargin = 6;
constexpr int kBarHeight = 28;
constexpr int kBarWidth = 320 - 2 * kFooterMargin;  // 308
constexpr int kBarTopY = 170 - kFooterMargin - kBarHeight;  // 136
// Deliberately not evenly divisible by kFooterItemCount: the floor
// division leaves a few px of slop past the last segment, absorbed by
// that segment instead of sitting unused past the bar's rounded edge
// (matches the Home footer's original "Power" segment behavior).
constexpr int kSegWidth = kBarWidth / kFooterItemCount;

}  // namespace

Footer::Footer(lv_obj_t* parent, const std::array<FooterItem, kFooterItemCount>& items, int highlightedIndex)
    : items_(items), highlightedIndex_(highlightedIndex) {
    bar_ = lv_obj_create(parent);
    lv_obj_set_size(bar_, kBarWidth, kBarHeight);
    lv_obj_set_pos(bar_, kFooterMargin, kBarTopY);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar_, 2, 0);
    lv_obj_set_style_border_color(bar_, theme::accent(), 0);
    lv_obj_set_style_radius(bar_, 8, 0);
    lv_obj_set_style_pad_all(bar_, 0, 0);
    lv_obj_clear_flag(bar_, LV_OBJ_FLAG_SCROLLABLE);

    rebuild();
}

void Footer::setItems(const std::array<FooterItem, kFooterItemCount>& items, int highlightedIndex) {
    items_ = items;
    highlightedIndex_ = highlightedIndex;
    rebuild();
}

void Footer::rebuild() {
    lv_obj_clean(bar_);

    for (int s = 0; s < kFooterItemCount; s++) {
        const int segWidth = (s == kFooterItemCount - 1) ? kBarWidth - s * kSegWidth : kSegWidth;
        const FooterItem& item = items_[s];
        const bool highlighted = (s == highlightedIndex_);
        const bool empty = (item.label == nullptr);

        lv_obj_t* seg = lv_obj_create(bar_);
        lv_obj_remove_style_all(seg);
        lv_obj_set_size(seg, segWidth, kBarHeight);
        lv_obj_set_pos(seg, s * kSegWidth, 0);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(seg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(seg, 2, 0);

        if (empty) {
            // No real action for this slot -- leave it visually blank
            // rather than inventing a fake label, still bordered by its
            // divider below so the five-section layout stays intact.
        } else if (highlighted) {
            // Inset a few px from the segment's own bounds so the fill
            // doesn't touch the outer bar's border/corner -- same treatment
            // as the original Home footer's highlighted first key.
            lv_obj_set_style_bg_color(seg, theme::accent(), 0);
            lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(seg, 5, 0);
            lv_obj_set_pos(seg, s * kSegWidth + 3, 3);
            lv_obj_set_size(seg, segWidth - 5, kBarHeight - 6);
        }

        if (s > 0) {
            lv_obj_t* divider = lv_obj_create(bar_);
            lv_obj_remove_style_all(divider);
            lv_obj_set_size(divider, 1, kBarHeight - 8);
            lv_obj_set_style_bg_color(divider, theme::accent(), 0);
            lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
            lv_obj_align(divider, LV_ALIGN_LEFT_MID, s * kSegWidth, 0);
        }

        if (empty) {
            continue;
        }

        const lv_color_t fg = highlighted ? lv_color_black() : lv_color_white();

        if (item.badge != nullptr) {
            // Fixed width (not LV_SIZE_CONTENT) so every badge is
            // pixel-identical regardless of its glyph's advance width.
            lv_obj_t* keyBox = lv_obj_create(seg);
            lv_obj_set_size(keyBox, 15, 13);
            lv_obj_set_style_bg_opa(keyBox, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(keyBox, 1, 0);
            lv_obj_set_style_border_color(keyBox, fg, 0);
            lv_obj_set_style_radius(keyBox, 2, 0);
            lv_obj_set_style_pad_all(keyBox, 0, 0);
            lv_obj_clear_flag(keyBox, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* keyLabel = lv_label_create(keyBox);
            lv_label_set_text(keyLabel, item.badge);
            lv_obj_set_style_text_font(keyLabel, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(keyLabel, fg, 0);
            lv_obj_center(keyLabel);
        }

        if (item.icon != nullptr) {
            lv_obj_t* icon = lv_label_create(seg);
            lv_label_set_text(icon, item.icon);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(icon, fg, 0);
        }

        lv_obj_t* textLabel = lv_label_create(seg);
        lv_label_set_text(textLabel, item.label);
        lv_obj_set_style_text_font(textLabel, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_letter_space(textLabel, -1, 0);
        lv_obj_set_style_text_color(textLabel, fg, 0);
    }
}

}  // namespace cardmesh::ui
