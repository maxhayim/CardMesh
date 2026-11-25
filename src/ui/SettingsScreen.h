#pragma once

#include <array>
#include <string>

#include "lvgl.h"
#include "storage/Settings.h"

namespace cardmesh::ui {

// Info: display-only, Enter does nothing. Action: Enter triggers a
// ui_main.cpp-owned side effect (checking/installing an update) instead of
// editing a cardmesh::storage::AppSettings field. Cycle: like Toggle, but
// Enter advances through more than two fixed values (e.g. LoraRegion)
// instead of just flipping a bool.
enum class SettingsFieldKind { Text, MaskedText, IntText, Toggle, Cycle, Info, Action };

// Fixed field order -- ui_main.cpp's commitSettingsField() switches on
// these same names, so keep the two in sync (the numeric order matters only
// for which section a field's row lands in, grouped via SettingsScreen.cpp's
// kFieldSpecs; see also addSectionHeader there).
enum class SettingsField {
    // General
    AutoUpdate = 0,
    HomeIsDashboard,
    Version,
    UpdateStatus,
    // MeshMonitor (REST connection + map tiles)
    Server,
    Port,
    Https,
    Token,
    TileServer,
    // Meshtastic / MeshCore: both share CardMesh's single direct-LoRa node
    // identity (settings.loraNodeNum) since they time-share one physical
    // radio -- see MeshtasticWorker.h -- so the same node number shows in
    // both sections; only the running/active status differs per protocol.
    MeshtasticNode,
    MeshtasticStatus,
    MeshCoreNode,
    MeshCoreStatus,
    LoraRegion,
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

    // The About section's two rows -- owned by ui_main.cpp (kVersion and
    // UpdateModel respectively), since SettingsScreen otherwise only knows
    // about cardmesh::storage::AppSettings. Call setUpdateStatusText() every
    // loop iteration same as DashboardScreen's updateStatusLine; it's cheap
    // (a string compare) and the status can change asynchronously from the
    // update worker thread at any time while Settings is open.
    void setVersionText(const std::string& text);
    void setUpdateStatusText(const std::string& text);

    // Meshtastic/MeshCore sections' "Status" row -- owned by ui_main.cpp
    // (MeshtasticWorker::isRunning()/MeshCoreWorker::isRunning()), same
    // reasoning as setVersionText() above.
    void setMeshtasticStatusText(const std::string& text);
    void setMeshCoreStatusText(const std::string& text);

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
    std::string versionText_ = "--";
    std::string updateStatusText_ = "--";
    std::string meshtasticStatusText_ = "--";
    std::string meshCoreStatusText_ = "--";
};

}  // namespace cardmesh::ui
