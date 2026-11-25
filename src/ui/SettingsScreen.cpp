#include "SettingsScreen.h"

#include <cstdint>
#include <cstdio>

#include "Theme.h"
#include "mesh/LoraRegions.h"

namespace cardmesh::ui {

namespace {

struct FieldSpec {
    const char* label;
    SettingsFieldKind kind;
    // Non-null => a section header with this text is inserted into the
    // list immediately before this row. Grouping is purely cosmetic (see
    // SettingsScreen::build()); it does not change selectedIndex_/rowLabels_
    // indexing, which stays a flat 0..kSettingsFieldCount-1 range.
    const char* sectionHeader;
};

constexpr FieldSpec kFieldSpecs[kSettingsFieldCount] = {
    {"Auto-update", SettingsFieldKind::Toggle, "General"},
    {"Home = icon grid (next boot)", SettingsFieldKind::Toggle, nullptr},
    {"Version", SettingsFieldKind::Info, nullptr},
    {"Update", SettingsFieldKind::Action, nullptr},
    {"Server", SettingsFieldKind::Text, "MeshMonitor"},
    {"Port", SettingsFieldKind::IntText, nullptr},
    {"HTTPS", SettingsFieldKind::Toggle, nullptr},
    {"Token", SettingsFieldKind::MaskedText, nullptr},
    {"Tile server", SettingsFieldKind::Text, nullptr},
    {"Node #", SettingsFieldKind::Info, "Meshtastic"},
    {"Status", SettingsFieldKind::Info, nullptr},
    {"Node #", SettingsFieldKind::Info, "MeshCore"},
    {"Status", SettingsFieldKind::Info, nullptr},
    {"Region", SettingsFieldKind::Cycle, "LoRa Region (Meshtastic + MeshCore)"},
};

std::string maskToken(const std::string& token) {
    if (token.empty()) {
        return "(not set)";
    }
    return "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2";  // bullet x4, UTF-8
}

std::string formatNodeNum(std::uint32_t nodeNum) {
    if (nodeNum == 0) {
        return "(not generated yet)";
    }
    char buf[12];
    std::snprintf(buf, sizeof(buf), "!%08x", nodeNum);
    return buf;
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
        case SettingsField::MeshtasticNode:
        case SettingsField::MeshCoreNode:
            return formatNodeNum(settings.loraNodeNum);
        case SettingsField::LoraRegion:
            return cardmesh::mesh::loraRegionDisplayName(settings.loraRegion);
        case SettingsField::Version:
        case SettingsField::UpdateStatus:
        case SettingsField::MeshtasticStatus:
        case SettingsField::MeshCoreStatus:
            break;  // rendered from ui_main.cpp-supplied text instead, see renderValues()
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
        if (kFieldSpecs[i].sectionHeader != nullptr) {
            // Plain ASCII, not a Unicode em-dash: the device's compiled-in
            // Montserrat font subset has no glyph for U+2014 and renders it
            // as a missing-glyph box -- caught via tools/screenshot before
            // this ever reached the device.
            const std::string headerText = "-- " + std::string(kFieldSpecs[i].sectionHeader) + " --";
            lv_obj_t* header = lv_list_add_text(list_, headerText.c_str());
            lv_obj_set_style_text_color(header, theme::accent(), 0);
        }
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
        const auto field = static_cast<SettingsField>(i);
        std::string value;
        if (field == SettingsField::Version) {
            value = versionText_;
        } else if (field == SettingsField::UpdateStatus) {
            value = updateStatusText_;
        } else if (field == SettingsField::MeshtasticStatus) {
            value = meshtasticStatusText_;
        } else if (field == SettingsField::MeshCoreStatus) {
            value = meshCoreStatusText_;
        } else {
            value = valueText(settings, i);
        }
        const std::string row = std::string(kFieldSpecs[i].label) + ": " + value;
        lv_label_set_text(rowLabels_[i], row.c_str());
    }
    updateSelectionHighlight();
}

void SettingsScreen::setVersionText(const std::string& text) {
    versionText_ = text;
}

void SettingsScreen::setUpdateStatusText(const std::string& text) {
    updateStatusText_ = text;
}

void SettingsScreen::setMeshtasticStatusText(const std::string& text) {
    meshtasticStatusText_ = text;
}

void SettingsScreen::setMeshCoreStatusText(const std::string& text) {
    meshCoreStatusText_ = text;
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
