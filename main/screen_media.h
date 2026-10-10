#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Media/volume tile skeleton: volume slider (static, real level via MQTT
 * in a later phase) + play/pause/prev/next buttons wired to the USB HID
 * Consumer Control transport.
 */
void screen_media_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
