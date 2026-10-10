#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Password/macro tile: PIN-gated list of saved text macros that get typed
 * via USB HID into whatever field has focus on the host PC. */
void screen_passwords_create(lv_obj_t *parent);

/* Re-locks the tile (call when the tileview navigates away from it). */
void screen_passwords_lock(void);

#ifdef __cplusplus
}
#endif
