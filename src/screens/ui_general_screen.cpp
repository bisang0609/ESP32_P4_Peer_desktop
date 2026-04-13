/**
 * General Settings Screen
 */

#include "ui_common.h"
#include "config.h"

// Forward declaration
lv_obj_t* createSettingsSidebar(lv_obj_t* screen, int activeIdx);

void createGeneralScreen() {
    scr_general = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_general, lv_color_hex(0x121212), 0);

    // Create sidebar and get content area (General is index 0)
    lv_obj_t* content = createSettingsSidebar(scr_general, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    // Title
    lv_obj_t* lbl_title = lv_label_create(content);
    lv_label_set_text(lbl_title, "General");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    lv_obj_set_style_pad_bottom(lbl_title, 16, 0);
}
