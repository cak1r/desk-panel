#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Now playing page: cover, title/artist/album, seekable progress,
 * transport + ±10 s, and the playing app's own volume. */
void screen_nowplaying_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
