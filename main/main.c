/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"
#include "usb_hid.h"
#include "hid_actions.h"
#include "wifi_time.h"
#include "app_mqtt.h"
#include "ui.h"

void app_main()
{
    waveshare_esp32_s3_rgb_lcd_init(); // Initialize the Waveshare ESP32-S3 RGB LCD
    wavesahre_rgb_lcd_bl_on();         // Turn on the screen backlight

    usb_hid_init();  // Bring up the USB HID (keyboard + consumer control) transport
    hid_actions_init(); // Worker task that sends HID shortcuts/macros off the LVGL task
    wifi_time_init(); // Connect to WiFi and sync the clock over SNTP in the background
    app_mqtt_init();  // Connect to the MQTT broker (no-op until main/mqtt_config.h is filled in)

    ESP_LOGI(TAG, "Starting UI");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lvgl_port_lock(-1)) {
        ui_init();

        lvgl_port_unlock();
    }
}
