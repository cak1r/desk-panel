#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* YouTube page: current video (from SMTC) plus YouTube's own keyboard
 * shortcuts sent over USB HID -- they act on whatever window has focus,
 * which is why the companion opens this page when a YouTube tab is focused. */
void screen_youtube_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
