#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Native USB OTG HID transport (TinyUSB device stack): composite
 * Keyboard + Consumer Control. No host-side software required --
 * the panel enumerates as a HID device over the same USB-C cable
 * used for power.
 */
void usb_hid_init(void);

/* Sends a single key press + release. `modifier` uses the HID_KEYBOARD_MODIFIER_* bitmask. */
void usb_hid_send_key(uint8_t modifier, uint8_t keycode);

/* Sends a single Consumer Control usage (volume, play/pause, next/prev, ...) press + release. */
void usb_hid_send_consumer(uint16_t usage);

/* Types out an ASCII string as a sequence of key press/release reports (US QWERTY layout). */
void usb_hid_send_text(const char *text);

#ifdef __cplusplus
}
#endif
