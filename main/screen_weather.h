#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Weather tile: current temperature + condition, feels-like/humidity/wind
 * cards. Fetched from Open-Meteo (see weather_state.c), no API key needed. */
void screen_weather_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
