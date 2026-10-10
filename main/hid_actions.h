#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Queues USB HID actions onto a worker task so touch handlers (which run
 * on the LVGL task) never block on key press/release delays -- a "launch
 * app" macro waits for the Run dialog, which would freeze the UI.
 */
void hid_actions_init(void);

/* One chord, e.g. Win+L: hid_actions_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_L). */
void hid_actions_key(uint8_t modifier, uint8_t keycode);

/* Consumer control usage (volume, media keys). */
void hid_actions_consumer(uint16_t usage);

/* Types UTF-8 text with the Turkish Q mapping of usb_hid_send_text(). */
void hid_actions_text(const char *text);

/* Win+R, waits for the Run dialog, types `command`, Enter. */
void hid_actions_launch(const char *command);

#ifdef __cplusplus
}
#endif
