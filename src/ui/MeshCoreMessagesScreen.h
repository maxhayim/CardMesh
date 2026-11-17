#pragma once

#include <cstddef>
#include <string>

#include "MeshCoreMessagesModel.h"
#include "lvgl.h"

namespace cardmesh::ui {

// View of direct MeshCore GRP_TXT traffic (see MeshCoreWorker). Composing
// is driven externally (ui_main.cpp), same pattern as
// LoraMessagesScreen/MeshMonitorMessagesScreen.
class MeshCoreMessagesScreen {
public:
    explicit MeshCoreMessagesScreen(MeshCoreMessagesModel& model);

    lv_obj_t* screenObj() const { return screen_; }

    void refresh();

    void setComposing(bool composing);
    void setComposeText(const std::string& text);

    // Updates the "Channel: X" label -- ui_main.cpp owns which channel is
    // currently selected for sending (see MeshCoreWorker::channelNames).
    void setChannelName(const std::string& name);

private:
    void build();

    MeshCoreMessagesModel& model_;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* channelLabel_ = nullptr;
    lv_obj_t* helpLabel_ = nullptr;
    lv_obj_t* list_ = nullptr;
    lv_obj_t* composeLabel_ = nullptr;

    std::size_t lastRenderedCount_ = 0;
    std::string lastStatusMessage_;
    bool composing_ = false;
};

}  // namespace cardmesh::ui
