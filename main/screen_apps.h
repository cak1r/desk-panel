#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Identifies which tile a grid button opens; passed as the event user_data
 * (see lv_event_get_user_data) to the callback given to screen_apps_create. */
enum {
    SCREEN_APPS_PRAYER = 0,
    SCREEN_APPS_CLAUDE,
    SCREEN_APPS_MEDIA,
    SCREEN_APPS_HEALTH,
    SCREEN_APPS_WEATHER,
    SCREEN_APPS_CLOCK,
};

/*
 * Home tile: a wrapping grid of buttons that open the namaz/Claude/media/PC
 * health/weather/clock screens full-tile. `on_app_clicked` fires on
 * LV_EVENT_CLICKED for every button with the SCREEN_APPS_* id as its
 * user_data.
 */
void screen_apps_create(lv_obj_t *parent, lv_event_cb_t on_app_clicked);

#ifdef __cplusplus
}
#endif
