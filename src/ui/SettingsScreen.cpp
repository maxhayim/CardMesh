#include "SettingsScreen.h"

#include <cstdio>

#include "Theme.h"

namespace cardmesh::ui {

namespace {

struct FieldSpec {
    const char* label;
    SettingsFieldKind kind;
};

constexpr FieldSpec kFieldSpecs[kSettingsFieldCount] = {
    {"Server", SettingsFieldKind::Text},
    {"Port", SettingsFieldKind::IntText},
    {"HTTPS", SettingsFieldKind::Toggle},
    {"Token", SettingsFieldKind::MaskedText},
    {"Tile server", SettingsFieldKind::Text},
    {"Auto-update", SettingsFieldKind::Toggle},
    {"Home = icon grid (next boot)", SettingsFieldKind::Toggle},
};

std::string maskToken(const std::string& token) {
    if (token.empty()) {
        return "(not set)";
    }
    return "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2";  // bullet x4, UTF-8
}

std::string valueText(const storage::AppSettings& settings, std::size_t index) {
    switch (static_cast<SettingsField>(index)) {
        case SettingsField::Server:
            return settings.server.empty() ? "(not set)" : settings.server;
        case SettingsField::Port: {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", settings.port);
            return buf;
        }
        case SettingsField::Https:
            return settings.https ? "ON" : "OFF";
        case SettingsField::Token:
            return maskToken(settings.apiToken);
        case SettingsField::TileServer:
            return settings.tileServerTemplate;
        case SettingsField::AutoUpdate:
            return settings.autoUpdateCheck ? "ON" : "OFF";
        case SettingsField::HomeIsDashboard:
            return settings.homeScreenIsDashboard ? "OFF" : "ON";
        case SettingsField::Count:
            break;
    }
    return "";
}

}  // namespace

SettingsScreen::SettingsScreen() { build(); }

void SettingsScreen::build() {
    screen_ = lv_obj_create(nullptr);
    lv_obj_t* screen = screen_;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 4, 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, theme::accent(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 2);

    theme::makeDivider(screen, 312, 2, 20);

    list_ = lv_list_create(screen);
    lv_obj_set_size(list_, 312, 112);
    lv_obj_align(list_, LV_ALIGN_TOP_LEFT, 2, 26);
    theme::applyCardStyle(list_);

    for (std::size_t i = 0; i < kSettingsFieldCount; i++) {
        const std::string row = std::string(kFieldSpecs[i].label) + ": --";
        rowLabels_[i] = lv_list_add_text(list_, row.c_str());
    }

    editLabel_ = lv_label_create(screen);
    lv_label_set_text(editLabel_, "");
    lv_obj_set_style_text_color(editLabel_, lv_color_white(), 0);
    lv_obj_align(editLabel_, LV_ALIGN_BOTTOM_LEFT, 2, -16);
    lv_obj_add_flag(editLabel_, LV_OBJ_FLAG_HIDDEN);

    helpLabel_ = lv_label_create(screen);
    lv_label_set_text(helpLabel_, "[F/X] select  [Enter] edit  [B]ack");
    lv_obj_set_style_text_color(helpLabel_, theme::dim(), 0);
    lv_obj_align(helpLabel_, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    updateSelectionHighlight();
}

void SettingsScreen::updateSelectionHighlight() {
    for (std::size_t i = 0; i < kSettingsFieldCount; i++) {
        const bool isSelected = (static_cast<int>(i) == selectedIndex_);
        lv_obj_set_style_text_color(rowLabels_[i], isSelected ? theme::accent() : lv_color_white(), 0);
    }
}

void SettingsScreen::moveUp() {
    selectedIndex_ = (selectedIndex_ + static_cast<int>(kSettingsFieldCount) - 1) % static_cast<int>(kSettingsFieldCount);
    updateSelectionHighlight();
}

void SettingsScreen::moveDown() {
    selectedIndex_ = (selectedIndex_ + 1) % static_cast<int>(kSettingsFieldCount);
    updateSelectionHighlight();
}

void SettingsScreen::renderValues(const storage::AppSettings& settings) {
    for (std::size_t i = 0; i < kSettingsFieldCount; i++) {
        const std::string row = std::string(kFieldSpecs[i].label) + ": " + valueText(settings, i);
        lv_label_set_text(rowLabels_[i], row.c_str());
    }
    updateSelectionHighlight();
}

void SettingsScreen::setEditing(bool editing) {
    editing_ = editing;
    if (editing) {
        lv_obj_clear_flag(editLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(helpLabel_, "[Enter] save  [Esc] cancel");
    } else {
        lv_obj_add_flag(editLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(helpLabel_, "[F/X] select  [Enter] edit  [B]ack");
    }
}

void SettingsScreen::setEditingText(const std::string& text) {
    const std::string display = "> " + text;
    lv_label_set_text(editLabel_, display.c_str());
}

SettingsFieldKind SettingsScreen::kindOf(SettingsField field) {
    return kFieldSpecs[static_cast<std::size_t>(field)].kind;
}

std::string SettingsScreen::editableValue(const storage::AppSettings& settings, SettingsField field) {
    if (field == SettingsField::Token) {
        return settings.apiToken;  // the real value, not maskToken()'s display form
    }
    return valueText(settings, static_cast<std::size_t>(field));
}

}  // namespace cardmesh::ui
