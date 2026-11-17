#include "DashboardScreen.h"

#include <cstdio>

#include "EmojiFont.h"
#include "Theme.h"

namespace cardmesh::ui {

namespace {

void setValueText(lv_obj_t* label, int value) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", value);
    lv_label_set_text(label, buf);
}

}  // namespace

DashboardScreen::DashboardScreen(DashboardModel& model) : model_(model) { build(); }

DashboardScreen::~DashboardScreen() { destroyEmojiFont(sourceLabelFont_); }

void DashboardScreen::build() {
    screen_ = lv_obj_create(nullptr);
    lv_obj_t* screen = screen_;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 4, 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "CardMesh");
    lv_obj_set_style_text_color(title, theme::accent(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 2);

    statusDot_ = lv_label_create(screen);
    lv_label_set_text(statusDot_, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(statusDot_, theme::warn(), 0);
    lv_obj_align(statusDot_, LV_ALIGN_TOP_RIGHT, -2, 2);

    theme::makeDivider(screen, 312, 2, 20);

    sourceLabel_ = lv_label_create(screen);
    lv_label_set_text(sourceLabel_, "SOURCE: --");
    lv_obj_set_style_text_color(sourceLabel_, lv_color_white(), 0);
    lv_obj_align(sourceLabel_, LV_ALIGN_TOP_LEFT, 2, 26);

    sourceLabelFont_ = createEmojiFont(16, LV_FONT_DEFAULT);
    if (sourceLabelFont_ != nullptr) {
        lv_obj_set_style_text_font(sourceLabel_, sourceLabelFont_, 0);
    }

    // A single rounded "card" behind the stat rows -- Meshtastic's own
    // node-list row styling, scaled up to hold the whole stat block rather
    // than one card per row (this screen is only 320x170).
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 312, 84);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, 2, 46);
    theme::applyCardStyle(card);

    struct Row {
        const char* label;
        int y;
        lv_obj_t** valueOut;
    };
    const Row rows[] = {
        {"Nodes", 4, &nodesValue_},
        {"Active", 24, &activeValue_},
        {"Channels", 44, &channelsValue_},
        {"Unread", 64, &unreadValue_},
    };

    for (const auto& row : rows) {
        lv_obj_t* nameLabel = lv_label_create(card);
        lv_label_set_text(nameLabel, row.label);
        lv_obj_set_style_text_color(nameLabel, theme::dim(), 0);
        lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, 6, row.y);

        lv_obj_t* valueLabel = lv_label_create(card);
        lv_label_set_text(valueLabel, "--");
        lv_obj_set_style_text_color(valueLabel, lv_color_white(), 0);
        lv_obj_align(valueLabel, LV_ALIGN_TOP_RIGHT, -6, row.y);
        *row.valueOut = valueLabel;
    }

    updateLabel_ = lv_label_create(screen);
    lv_label_set_text(updateLabel_, "");
    lv_obj_set_style_text_color(updateLabel_, lv_color_make(0xff, 0xcc, 0x00), 0);
    lv_obj_align(updateLabel_, LV_ALIGN_BOTTOM_LEFT, 2, -16);

    statusLabel_ = lv_label_create(screen);
    lv_label_set_text(statusLabel_, "[R]efresh [M]ap [T]ext [K]ore [S]ettings [Q]uit");
    lv_obj_set_style_text_color(statusLabel_, theme::dim(), 0);
    lv_obj_align(statusLabel_, LV_ALIGN_BOTTOM_LEFT, 2, -2);
}

void DashboardScreen::refresh() {
    bool connected;
    std::string sourceName;
    std::string statusMessage;
    int nodeCount;
    int activeCount;
    int channelCount;
    int unreadCount;
    std::string updateStatusLine;

    {
        std::lock_guard<std::mutex> lock(model_.mutex);
        connected = model_.connected;
        sourceName = model_.sourceName;
        statusMessage = model_.statusMessage;
        nodeCount = model_.nodeCount;
        activeCount = model_.activeCount;
        channelCount = model_.channelCount;
        unreadCount = model_.unreadCount;
        updateStatusLine = model_.updateStatusLine;
    }

    if (connected != lastConnected_) {
        lv_label_set_text(statusDot_, connected ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(statusDot_, connected ? theme::accent() : theme::danger(), 0);
        lastConnected_ = connected;
    }
    if (sourceName != lastSourceName_ || statusMessage != lastStatusMessage_) {
        const std::string text =
            connected ? ("SOURCE: " + sourceName) : (sourceName.empty() ? statusMessage : sourceName);
        lv_label_set_text(sourceLabel_, text.c_str());
        lastSourceName_ = sourceName;
        lastStatusMessage_ = statusMessage;
    }
    if (nodeCount != lastNodeCount_) {
        setValueText(nodesValue_, nodeCount);
        lastNodeCount_ = nodeCount;
    }
    if (activeCount != lastActiveCount_) {
        setValueText(activeValue_, activeCount);
        lastActiveCount_ = activeCount;
    }
    if (channelCount != lastChannelCount_) {
        setValueText(channelsValue_, channelCount);
        lastChannelCount_ = channelCount;
    }
    if (unreadCount != lastUnreadCount_) {
        setValueText(unreadValue_, unreadCount);
        // MeshCore's unread-badge convention: draw attention in red when
        // there's something new, otherwise it's just another plain stat.
        lv_obj_set_style_text_color(unreadValue_, unreadCount > 0 ? theme::danger() : lv_color_white(), 0);
        lastUnreadCount_ = unreadCount;
    }
    if (updateStatusLine != lastUpdateStatusLine_) {
        lv_label_set_text(updateLabel_, updateStatusLine.c_str());
        lastUpdateStatusLine_ = updateStatusLine;
    }
}

}  // namespace cardmesh::ui
