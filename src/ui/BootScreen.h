#pragma once

#include "lvgl.h"

namespace cardmesh::ui {

// A brief splash screen shown once at startup, before the Home/Dashboard
// screen loads -- see src/ui_main.cpp for the timed transition out of it.
// Static content, no model to read, so there's no refresh().
class BootScreen {
public:
    BootScreen();

    lv_obj_t* screenObj() const { return screen_; }

private:
    void build();

    lv_obj_t* screen_ = nullptr;
};

}  // namespace cardmesh::ui
