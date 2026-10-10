#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Prayer-times tile skeleton: countdown header + 6 time slots.
 * Placeholder data only -- Aladhan API fetch + NVS cache lands in a later phase.
 */
void screen_prayer_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
