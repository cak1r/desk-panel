#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Claude API usage tile skeleton: request/token stat cards.
 * Placeholder data only -- real numbers arrive via MQTT from the PC companion service later.
 */
void screen_claude_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
