idf_component_register(
    SRCS   "ui.c" "ui_theme.c" "ui_art.c" "ui_format.c"
           "screen_clock.c" "screen_deck.c" "screen_nowplaying.c" "screen_mixer.c" "screen_youtube.c"
           "screen_whatsapp.c" "screen_prayer.c" "screen_claude.c" "screen_pc_health.c" "screen_weather.c"
           "screen_passwords.c"
           "usb_hid.c" "wifi_time.c" "prayer_times.c" "app_mqtt.c" "claude_usage.c" "audio_state.c"
           "pc_health_state.c" "weather_state.c"
           "media_state.c" "mixer_state.c" "profile_state.c" "whatsapp_state.c" "hadith_state.c" "hid_actions.c"
           "waveshare_rgb_lcd_port.c" "main.c" "lvgl_port.c"
           "fonts/manrope_medium_13.c" "fonts/manrope_medium_15.c" "fonts/manrope_medium_18.c"
           "fonts/manrope_light_20.c" "fonts/manrope_medium_26.c" "fonts/manrope_medium_32.c"
           "fonts/manrope_medium_60_num.c" "fonts/manrope_medium_163_num.c"
           "fonts/tabler_icons_18.c" "fonts/tabler_icons_24.c" "fonts/tabler_icons_36.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer esp_wifi esp_netif esp_event nvs_flash lwip esp_http_client json mbedtls mqtt)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
