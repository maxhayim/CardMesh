#pragma once

#include <cstddef>
#include <string>

#include "MeshMonitorMessagesModel.h"
#include "lvgl.h"

namespace cardmesh::ui {

// View of messages relayed through the MeshMonitor server (see
// NetworkWorker). Deliberately separate from CardMesh's direct-LoRa
// message list -- the two mesh paths are not merged.
//
// Composing is driven externally (ui_main.cpp), same pattern as
// LoraMessagesScreen; this class only renders the compose line's text.
class MeshMonitorMessagesScreen {
public:
    explicit MeshMonitorMessagesScreen(MeshMonitorMessagesModel& model);

    lv_obj_t* screenObj() const { return screen_; }

    // Call periodically from the LVGL timer/main loop; cheap no-op if
    // nothing has changed since the last call.
    void refresh();

    void setComposing(bool composing);
    void setComposeText(const std::string& text);

private:
    void build();

    MeshMonitorMessagesModel& model_;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* helpLabel_ = nullptr;
    lv_obj_t* list_ = nullptr;
    lv_obj_t* composeLabel_ = nullptr;

    std::size_t lastRenderedCount_ = 0;
    bool composing_ = false;
};

}  // namespace cardmesh::ui
