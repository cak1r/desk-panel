#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PC health tile: CPU/RAM/GPU usage cards, fed by the companion over MQTT. */
void screen_pc_health_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
