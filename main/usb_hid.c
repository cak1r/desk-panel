#include "usb_hid.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "tinyusb.h"
#include "class/hid/hid_device.h"

#define HID_RID_KEYBOARD 1
#define HID_RID_CONSUMER 2

static const char *TAG = "usb_hid";

static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_RID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(HID_RID_CONSUMER)),
};

static const char *s_hid_string_descriptor[5] = {
    (const char[]){ 0x09, 0x04 }, /* 0: language = English (0x0409) */
    "Deskpanel",                  /* 1: manufacturer */
    "ESP32-S3 Desktop Panel",     /* 2: product */
    "0",                          /* 3: serial number */
    "Desktop Panel HID",          /* 4: HID interface name */
};

static const uint8_t s_hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN,
                           TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(s_hid_report_descriptor), 0x81, 16, 10),
};

/* --- TinyUSB HID class callbacks --- */

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

void usb_hid_init(void)
{
    ESP_LOGI(TAG, "Installing TinyUSB HID driver");

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = s_hid_string_descriptor,
        .string_descriptor_count = sizeof(s_hid_string_descriptor) / sizeof(s_hid_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = s_hid_configuration_descriptor,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
}

static bool wait_hid_ready(void)
{
    for (int i = 0; i < 50; i++) {
        if (tud_hid_ready()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return false;
}

void usb_hid_send_key(uint8_t modifier, uint8_t keycode)
{
    if (!tud_mounted()) {
        return;
    }

    uint8_t keycodes[6] = { keycode, 0, 0, 0, 0, 0 };

    if (wait_hid_ready()) {
        tud_hid_keyboard_report(HID_RID_KEYBOARD, modifier, keycodes);
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    if (wait_hid_ready()) {
        tud_hid_keyboard_report(HID_RID_KEYBOARD, 0, NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

void usb_hid_send_consumer(uint16_t usage)
{
    if (!tud_mounted()) {
        return;
    }

    if (wait_hid_ready()) {
        tud_hid_report(HID_RID_CONSUMER, &usage, sizeof(usage));
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    uint16_t release = 0;
    if (wait_hid_ready()) {
        tud_hid_report(HID_RID_CONSUMER, &release, sizeof(release));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

static const uint8_t s_ascii_to_keycode[128][2] = { HID_ASCII_TO_KEYCODE };

/*
 * Characters that live on different physical keys (or need a different
 * modifier) on the Windows "Turkish Q" layout versus the plain US table
 * above. Checked first, so this both adds the Turkish letters and
 * corrects ASCII characters whose key moved (e.g. on Turkish Q the
 * US-I-key position types dotless "i" unshifted, not plain ASCII "i";
 * the whole digit-row/punctuation shift and AltGr assignment differs
 * from US). '.', '?', and '@' below are confirmed against real hardware
 * (2026-08-18); the rest follow the same well-documented Turkish Q
 * digit-row pattern (1..0 shifted = ! ' ^ + % & / ( ) =).
 */
typedef struct {
    uint32_t codepoint;
    uint8_t modifier;
    uint8_t keycode;
} tr_key_map_t;

static const tr_key_map_t s_tr_key_map[] = {
    /* Turkish letters -- dedicated keys, no AltGr needed. */
    { 0x0131, 0, HID_KEY_I },                                  /* dotless i, lowercase */
    { 0x0049, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_I },        /* I, capital (dotless, same glyph as ASCII) */
    { 0x0069, 0, HID_KEY_APOSTROPHE },                         /* i, dotted lowercase (plain ASCII i) */
    { 0x0130, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_APOSTROPHE }, /* I with dot above, capital */
    { 0x00E7, 0, HID_KEY_PERIOD },                             /* c with cedilla, lowercase */
    { 0x00C7, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_PERIOD },   /* C with cedilla, uppercase */
    { 0x011F, 0, HID_KEY_BRACKET_LEFT },                       /* g with breve, lowercase */
    { 0x011E, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_BRACKET_LEFT }, /* G with breve, uppercase */
    { 0x00F6, 0, HID_KEY_COMMA },                              /* o with diaeresis, lowercase */
    { 0x00D6, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_COMMA },    /* O with diaeresis, uppercase */
    { 0x015F, 0, HID_KEY_SEMICOLON },                          /* s with cedilla, lowercase */
    { 0x015E, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_SEMICOLON }, /* S with cedilla, uppercase */
    { 0x00FC, 0, HID_KEY_BRACKET_RIGHT },                      /* u with diaeresis, lowercase */
    { 0x00DC, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_BRACKET_RIGHT }, /* U with diaeresis, uppercase */

    /* Digit row + punctuation -- Turkish Q shift/AltGr differs from US. */
    { '!', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_1 },
    { '\'', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_2 },
    { '^', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_3 },
    { '#', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_3 },
    { '+', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_4 },
    { '$', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_4 },
    { '%', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_5 },
    { '&', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_6 },
    { '/', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_7 },
    { '{', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_7 },
    { '(', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_8 },
    { '[', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_8 },
    { ')', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_9 },
    { ']', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_9 },
    { '=', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_0 },
    { '}', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_0 },
    { '*', 0, HID_KEY_MINUS },
    { '?', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_MINUS },       /* confirmed on hardware */
    { '\\', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_MINUS },
    { '-', 0, HID_KEY_EQUAL },
    { '_', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_EQUAL },
    { '|', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_EQUAL },
    { '.', 0, HID_KEY_SLASH },                                 /* confirmed on hardware */
    { ':', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_SLASH },
    { ',', 0, HID_KEY_BACKSLASH },
    { ';', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_BACKSLASH },
    { '`', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_BACKSLASH },
    { '@', KEYBOARD_MODIFIER_RIGHTALT, HID_KEY_Q },            /* confirmed on hardware */
    /* The ISO key left of Z (YouTube's speed shortcuts are '<' / '>'). */
    { '<', 0, HID_KEY_EUROPE_2 },
    { '>', KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_EUROPE_2 },
};

static bool lookup_tr_key(uint32_t codepoint, uint8_t *modifier, uint8_t *keycode)
{
    for (size_t i = 0; i < sizeof(s_tr_key_map) / sizeof(s_tr_key_map[0]); i++) {
        if (s_tr_key_map[i].codepoint == codepoint) {
            *modifier = s_tr_key_map[i].modifier;
            *keycode = s_tr_key_map[i].keycode;
            return true;
        }
    }
    return false;
}

void usb_hid_send_text(const char *text)
{
    if (!text) {
        return;
    }

    const unsigned char *p = (const unsigned char *)text;
    while (*p != '\0') {
        uint32_t codepoint;
        int len;

        if (*p < 0x80) {
            codepoint = *p;
            len = 1;
        } else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            codepoint = ((uint32_t)(*p & 0x1F) << 6) | (p[1] & 0x3F);
            len = 2;
        } else {
            p++; /* unsupported/invalid UTF-8 sequence -- skip a byte and resync */
            continue;
        }

        uint8_t modifier = 0, keycode = 0;
        if (lookup_tr_key(codepoint, &modifier, &keycode)) {
            usb_hid_send_key(modifier, keycode);
        } else if (codepoint < 128) {
            uint8_t shift = s_ascii_to_keycode[codepoint][0];
            keycode = s_ascii_to_keycode[codepoint][1];
            if (keycode != 0) {
                usb_hid_send_key(shift ? KEYBOARD_MODIFIER_LEFTSHIFT : 0, keycode);
            }
        }
        /* else: unsupported character -- skip */

        p += len;
    }
}
