#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ui_metadata_fonts_init(void);

extern lv_font_t ui_font_title_chain;
extern lv_font_t ui_font_artist_chain;
extern lv_font_t ui_font_next_title_chain;
extern lv_font_t ui_font_next_artist_chain;

#ifdef __cplusplus
}
#endif
