#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mixer page: a vertical fader + mute button per app with an active audio
 * session, plus the output device and the microphone. */
void screen_mixer_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
