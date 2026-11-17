#pragma once

#include <array>
#include <string>

#include "lvgl.h"
#include "storage/Settings.h"

namespace cardmesh::ui {

enum class SettingsFieldKind { Text, MaskedText, IntText, Toggle };

// Fixed field order -- ui_main.cpp's commitSettingsField() switches on
// these same indices, so keep the two in sync.
enum class SettingsField {
    Server = 0,
    Port,
    Https,
    Token,
    TileServer,
    AutoUpdate,
    HomeIsDashboard,
    Count,
};

constexpr std::size_t kSettingsFieldCount = static_cast<std::size_t>(SettingsField::Count);

// On-device editor for connection config and app-level toggles -- the
// first-run setup previously only possible by hand-editing config.json
// over SSH (see storage::Settings). Navigate rows with Up/Down, Enter to
// edit a text field or flip a toggle immediately; composing a text edit is
// driven externally by ui_main.cpp, same pattern as the message screens.
class SettingsScreen {
public:
    SettingsScreen();

    lv_obj_t* screenObj() const { return screen_; }

    void moveUp();
    void moveDown();
    SettingsField selected() const { return static_cast<SettingsField>(selectedIndex_); }

    // Re-renders every row's displayed value from the given settings.
    void renderValues(const storage::AppSettings& settings);

    // Whether the selected field is a toggle (Enter flips it immediately)
    // or a text field (Enter starts editing it) -- lets ui_main.cpp decide
    // without duplicating SettingsScreen.cpp's field table.
    static SettingsFieldKind kindOf(SettingsField field);

    // Current displayed value for a text-kind field, as the starting point
    // for an edit (Token returns the REAL value, not the masked display).
    static std::string editableValue(const storage::AppSettings& settings, SettingsField field);

    // Shows/hides the edit line while editing the selected field's value.
    void setEditing(bool editing);
    void setEditingText(const std::string& text);

private:
    void build();
    void updateSelectionHighlight();

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* helpLabel_ = nullptr;
    lv_obj_t* editLabel_ = nullptr;
    lv_obj_t* list_ = nullptr;
    std::array<lv_obj_t*, kSettingsFieldCount> rowLabels_{};

    int selectedIndex_ = 0;
    bool editing_ = false;
};

}  // namespace cardmesh::ui
