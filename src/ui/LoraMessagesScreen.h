#pragma once

#include <cstddef>
#include <string>

#include "LoraMessagesModel.h"
#include "lvgl.h"

namespace cardmesh::ui {

// Read-only view of CardMesh's direct-LoRa message log (see
// MeshtasticWorker). Deliberately separate from MeshMonitor's own message
// list, by design -- the two mesh paths are not merged. Sending from this
// screen is not implemented yet; see MeshtasticWorker::sendText.
class LoraMessagesScreen {
public:
    explicit LoraMessagesScreen(LoraMessagesModel& model);

    lv_obj_t* screenObj() const { return screen_; }

    // Call periodically from the LVGL timer/main loop; cheap no-op if no
    // new messages have arrived since the last call.
    void refresh();

private:
    void build();

    LoraMessagesModel& model_;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* list_ = nullptr;

    std::size_t lastRenderedCount_ = 0;
    std::string lastStatusMessage_;
};

}  // namespace cardmesh::ui
