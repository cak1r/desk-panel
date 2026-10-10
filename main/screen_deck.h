#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Home tile: a Stream Deck style 5x3 key grid. App keys fire USB HID
 * shortcuts or open pages; live keys show companion/HTTP data (now
 * playing, WhatsApp notifications, mic state, Claude usage, next prayer,
 * weather, volume, CPU) and open the matching page.
 */
void screen_deck_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
