idf_component_register(
    SRCS   "ui.c" "ui_theme.c"
           "screen_clock.c" "screen_apps.c" "screen_prayer.c" "screen_claude.c" "screen_media.c"
           "screen_pc_health.c" "screen_weather.c" "screen_passwords.c"
           "usb_hid.c" "wifi_time.c" "prayer_times.c" "app_mqtt.c" "claude_usage.c" "audio_state.c"
           "pc_health_state.c" "weather_state.c"
           "waveshare_rgb_lcd_port.c" "main.c" "lvgl_port.c"
           "fonts/manrope_medium_163_num.c" "fonts/manrope_medium_32.c"
           "fonts/manrope_light_20.c" "fonts/tabler_icons_36.c"
           "images/bg_gradient.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash lwip esp_http_client json mbedtls mqtt)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
