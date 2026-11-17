#include "BootScreen.h"

#include "Theme.h"

namespace cardmesh::ui {

BootScreen::BootScreen() { build(); }

void BootScreen::build() {
    screen_ = lv_obj_create(nullptr);
    lv_obj_t* screen = screen_;
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "CardMesh");
    lv_obj_set_style_text_color(title, theme::accent(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t* tagline = lv_label_create(screen);
    lv_label_set_text(tagline, "Your pocket console for the mesh.");
    lv_obj_set_style_text_color(tagline, theme::dim(), 0);
    lv_obj_align_to(tagline, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
}

}  // namespace cardmesh::ui
