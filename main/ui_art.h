#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Album cover shared by every screen that shows it (Deck key, clock chip,
 * now playing). The cover arrives once over MQTT; each widget draws the
 * same image buffer scaled to its own size, with a music-note placeholder
 * while there is no cover for the current track.
 */

/* Rounded square of `size` px showing the current cover. */
lv_obj_t *ui_art_create(lv_obj_t *parent, int size);

/* Pulls a newer cover from media_state if any and refreshes all widgets.
 * Called periodically by ui.c on the LVGL task. */
void ui_art_poll(void);

#ifdef __cplusplus
}
#endif
