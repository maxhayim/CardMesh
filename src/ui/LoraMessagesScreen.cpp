#include "LoraMessagesScreen.h"

#include <array>
#include <cstdio>

#include "Theme.h"

namespace cardmesh::ui {

namespace {

std::string formatRow(const LoraMessageEntry& entry) {
    if (entry.outgoing) {
        return ">> " + entry.text;
    }
    char idBuf[12];
    std::snprintf(idBuf, sizeof(idBuf), "%08x", entry.fromNode);
    return "!" + std::string(idBuf) + ": " + entry.text;
}

}  // namespace

namespace {

// Browsing: the screen's real bindings are Back('B'), channel cycle
// (Z/C -- PanLeft/PanRight, see ui_main.cpp), and StartCompose(Enter).
// There's no 5th real action on this screen today, so that slot is left
// blank rather than inventing one -- see Footer.h on why fake items aren't
// used to fill out the row.
constexpr std::array<FooterItem, kFooterItemCount> kBrowsingFooterItems = {{
    {"F1", LV_SYMBOL_HOME, "Home"},
    {"F2", LV_SYMBOL_LEFT, "Ch -"},
    {"F3", LV_SYMBOL_RIGHT, "Ch +"},
    {"F4", LV_SYMBOL_EDIT, "Write"},
    {},
}};

// Composing: channel cycling is disabled while typing (Z/C are message
// text, not shortcuts -- see EvdevKeyboard::pollText), so those slots go
// blank instead of showing a binding that's temporarily inactive.
constexpr std::array<FooterItem, kFooterItemCount> kComposingFooterItems = {{
    {"F1", LV_SYMBOL_CLOSE, "Cancel"},
    {},
    {},
    {"F4", LV_SYMBOL_OK, "Send"},
    {},
}};

}  // namespace

LoraMessagesScreen::LoraMessagesScreen(LoraMessagesModel& model) : model_(model) { build(); }

void LoraMessagesScreen::build() {
    screen_ = lv_obj_create(nullptr);
    lv_obj_t* screen = screen_;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Direct LoRa");
    lv_obj_set_style_text_color(title, theme::accent(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

    channelLabel_ = lv_label_create(screen);
    lv_label_set_text(channelLabel_, "Channel: --");
    lv_obj_set_style_text_color(channelLabel_, theme::dim(), 0);
    lv_obj_align(channelLabel_, LV_ALIGN_TOP_RIGHT, -8, 4);

    theme::makeDivider(screen, 304, 8, 22);

    list_ = lv_list_create(screen);
    lv_obj_set_size(list_, 304, 84);
    lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 8, 28);
    theme::applyCardStyle(list_);

    statusLabel_ = lv_label_create(screen);
    lv_label_set_text(statusLabel_, "LORA: STARTING");
    lv_obj_set_style_text_color(statusLabel_, theme::warn(), 0);
    lv_obj_align(statusLabel_, LV_ALIGN_TOP_LEFT, 8, 116);

    composeLabel_ = lv_label_create(screen);
    lv_label_set_text(composeLabel_, "");
    lv_obj_set_style_text_color(composeLabel_, lv_color_white(), 0);
    lv_obj_align(composeLabel_, LV_ALIGN_TOP_LEFT, 8, 116);
    lv_obj_add_flag(composeLabel_, LV_OBJ_FLAG_HIDDEN);

    footer_ = std::make_unique<Footer>(screen, kBrowsingFooterItems);
}

void LoraMessagesScreen::setComposing(bool composing) {
    composing_ = composing;
    if (composing) {
        lv_obj_add_flag(statusLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(composeLabel_, LV_OBJ_FLAG_HIDDEN);
        footer_->setItems(kComposingFooterItems);
    } else {
        lv_obj_clear_flag(statusLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(composeLabel_, LV_OBJ_FLAG_HIDDEN);
        footer_->setItems(kBrowsingFooterItems);
    }
}

void LoraMessagesScreen::setChannelName(const std::string& name) {
    const std::string text = "Channel: " + name;
    lv_label_set_text(channelLabel_, text.c_str());
}

void LoraMessagesScreen::setComposeText(const std::string& text) {
    const std::string display = "> " + text;
    lv_label_set_text(composeLabel_, display.c_str());
}

void LoraMessagesScreen::refresh() {
    std::size_t count;
    std::string statusMessage;
    std::vector<LoraMessageEntry> newEntries;

    {
        std::lock_guard<std::mutex> lock(model_.mutex);
        count = model_.messages.size();
        statusMessage = model_.statusMessage;
        if (count > lastRenderedCount_) {
            newEntries.assign(model_.messages.begin() + static_cast<long>(lastRenderedCount_), model_.messages.end());
        }
    }

    if (statusMessage != lastStatusMessage_) {
        lv_label_set_text(statusLabel_, statusMessage.c_str());
        lastStatusMessage_ = statusMessage;
    }

    if (newEntries.empty()) {
        return;
    }

    for (const auto& entry : newEntries) {
        const std::string row = formatRow(entry);
        lv_obj_t* rowLabel = lv_list_add_text(list_, row.c_str());
        // lv_list's text-row class carries LVGL's own default (light)
        // theme background -- without this override it paints a pale
        // panel behind each row, leaving little contrast against white/
        // accent text. Transparent lets the dark card background (set on
        // list_ via theme::applyCardStyle) show through instead.
        lv_obj_set_style_bg_opa(rowLabel, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(rowLabel, entry.outgoing ? theme::accent() : lv_color_white(), 0);
    }
    lastRenderedCount_ = count;

    lv_obj_scroll_to_y(list_, LV_COORD_MAX, LV_ANIM_OFF);
}

}  // namespace cardmesh::ui
