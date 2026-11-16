#include "LoraMessagesScreen.h"

#include <cstdio>

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

LoraMessagesScreen::LoraMessagesScreen(LoraMessagesModel& model) : model_(model) { build(); }

void LoraMessagesScreen::build() {
    screen_ = lv_obj_create(nullptr);
    lv_obj_t* screen = screen_;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 4, 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "DIRECT LORA");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 2);

    lv_obj_t* divider = lv_obj_create(screen);
    lv_obj_set_size(divider, 312, 1);
    lv_obj_set_style_bg_color(divider, lv_color_white(), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 2, 20);

    list_ = lv_list_create(screen);
    lv_obj_set_size(list_, 312, 112);
    lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 2, 26);
    lv_obj_set_style_bg_color(list_, lv_color_black(), 0);
    lv_obj_set_style_border_width(list_, 0, 0);

    statusLabel_ = lv_label_create(screen);
    lv_label_set_text(statusLabel_, "LORA: STARTING");
    lv_obj_set_style_text_color(statusLabel_, lv_color_make(0xff, 0xcc, 0x00), 0);
    lv_obj_align(statusLabel_, LV_ALIGN_BOTTOM_LEFT, 2, -16);

    lv_obj_t* help = lv_label_create(screen);
    lv_label_set_text(help, "[B]ack  (sending not implemented yet)");
    lv_obj_set_style_text_color(help, lv_color_white(), 0);
    lv_obj_align(help, LV_ALIGN_BOTTOM_LEFT, 2, -2);
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
        lv_list_add_text(list_, row.c_str());
    }
    lastRenderedCount_ = count;

    lv_obj_scroll_to_y(list_, LV_COORD_MAX, LV_ANIM_OFF);
}

}  // namespace cardmesh::ui
