#pragma once

#include <cstddef>
#include <string>

#include "LoraMessagesModel.h"
#include "lvgl.h"

namespace cardmesh::ui {

// View of CardMesh's direct-LoRa message log (see MeshtasticWorker).
// Deliberately separate from MeshMonitor's own message list, by design --
// the two mesh paths are not merged.
//
// Composing is driven externally (ui_main.cpp owns the enter/cancel/submit
// keyboard logic, since that's shared with EvdevKeyboard's mode switch);
// this class only renders the compose line's current text.
class LoraMessagesScreen {
public:
    explicit LoraMessagesScreen(LoraMessagesModel& model);

    lv_obj_t* screenObj() const { return screen_; }

    // Call periodically from the LVGL timer/main loop; cheap no-op if
    // nothing has changed since the last call.
    void refresh();

    // Shows/hides the compose line and its help text.
    void setComposing(bool composing);

    // Updates the compose line's displayed text (only meaningful while
    // setComposing(true) is active).
    void setComposeText(const std::string& text);

private:
    void build();

    LoraMessagesModel& model_;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* helpLabel_ = nullptr;
    lv_obj_t* list_ = nullptr;
    lv_obj_t* composeLabel_ = nullptr;

    std::size_t lastRenderedCount_ = 0;
    std::string lastStatusMessage_;
    bool composing_ = false;
};

}  // namespace cardmesh::ui
