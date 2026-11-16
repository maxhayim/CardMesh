#include "MeshMonitorMessagesScreen.h"

namespace cardmesh::ui {

namespace {

std::string formatRow(const models::Message& message) {
    return message.senderName + ": " + message.text;
}

}  // namespace

MeshMonitorMessagesScreen::MeshMonitorMessagesScreen(MeshMonitorMessagesModel& model) : model_(model) { build(); }

void MeshMonitorMessagesScreen::build() {
    screen_ = lv_obj_create(nullptr);
    lv_obj_t* screen = screen_;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 4, 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "MESHMONITOR");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 2);

    lv_obj_t* divider = lv_obj_create(screen);
    lv_obj_set_size(divider, 312, 1);
    lv_obj_set_style_bg_color(divider, lv_color_white(), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 2, 20);

    list_ = lv_list_create(screen);
    lv_obj_set_size(list_, 312, 120);
    lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 2, 26);
    lv_obj_set_style_bg_color(list_, lv_color_black(), 0);
    lv_obj_set_style_border_width(list_, 0, 0);

    composeLabel_ = lv_label_create(screen);
    lv_label_set_text(composeLabel_, "");
    lv_obj_set_style_text_color(composeLabel_, lv_color_white(), 0);
    lv_obj_align(composeLabel_, LV_ALIGN_BOTTOM_LEFT, 2, -16);
    lv_obj_add_flag(composeLabel_, LV_OBJ_FLAG_HIDDEN);

    helpLabel_ = lv_label_create(screen);
    lv_label_set_text(helpLabel_, "[Enter] compose  [B]ack");
    lv_obj_set_style_text_color(helpLabel_, lv_color_white(), 0);
    lv_obj_align(helpLabel_, LV_ALIGN_BOTTOM_LEFT, 2, -2);
}

void MeshMonitorMessagesScreen::setComposing(bool composing) {
    composing_ = composing;
    if (composing) {
        lv_obj_clear_flag(composeLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(helpLabel_, "[Enter] send  [Esc] cancel");
    } else {
        lv_obj_add_flag(composeLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(helpLabel_, "[Enter] compose  [B]ack");
    }
}

void MeshMonitorMessagesScreen::setComposeText(const std::string& text) {
    const std::string display = "> " + text;
    lv_label_set_text(composeLabel_, display.c_str());
}

void MeshMonitorMessagesScreen::refresh() {
    std::size_t count;
    std::vector<models::Message> newEntries;

    {
        std::lock_guard<std::mutex> lock(model_.mutex);
        count = model_.messages.size();
        if (count > lastRenderedCount_) {
            newEntries.assign(model_.messages.begin() + static_cast<long>(lastRenderedCount_), model_.messages.end());
        }
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
