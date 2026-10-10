#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Home tile: big clock + small date. Auto-updates every second from the system clock. */
void screen_clock_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
