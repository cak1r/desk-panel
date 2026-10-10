#include "screen_weather.h"
#include "weather_state.h"
#include "ui_theme.h"

static lv_obj_t *s_temp_label;
static lv_obj_t *s_condition_label;
static lv_obj_t *s_feels_label;
static lv_obj_t *s_humidity_label;
static lv_obj_t *s_wind_label;

static lv_obj_t *create_stat_card(lv_obj_t *parent, const char *title, lv_obj_t **value_label_out)
{
    lv_obj_t *card = lv_obj_create(parent);
    ui_theme_apply_card(card);
    lv_obj_set_size(card, 200, 140);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 8, 0);

    lv_obj_t *title_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(title_lbl, ui_theme_font_body(), 0);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x8a8f98), 0);

    lv_obj_t *value_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(value_lbl, ui_theme_font_emphasis(), 0);
    lv_label_set_text(value_lbl, "--");
    lv_obj_set_style_text_color(value_lbl, ui_theme_accent_color(), 0);
    *value_label_out = value_lbl;

    return card;
}

static void weather_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    weather_snapshot_t snap;
    weather_state_get_snapshot(&snap);

    if (snap.have_data) {
        lv_label_set_text_fmt(s_temp_label, "%d C", snap.temperature_c);
        lv_label_set_text(s_condition_label, weather_state_condition_text(snap.weather_code));
        lv_label_set_text_fmt(s_feels_label, "%d C", snap.feels_like_c);
        lv_label_set_text_fmt(s_humidity_label, "%d%%", snap.humidity_percent);
        lv_label_set_text_fmt(s_wind_label, "%d km/s", snap.wind_kmh);
    } else {
        lv_label_set_text(s_temp_label, "-- C");
        lv_label_set_text(s_condition_label, "Yukleniyor...");
        lv_label_set_text(s_feels_label, "--");
        lv_label_set_text(s_humidity_label, "--");
        lv_label_set_text(s_wind_label, "--");
    }
}

void screen_weather_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, ui_theme_bg_color(), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 16, 0);

    s_temp_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_temp_label, ui_theme_font_emphasis(), 0);
    lv_obj_set_style_text_color(s_temp_label, lv_color_white(), 0);
    lv_label_set_text(s_temp_label, "-- C");

    s_condition_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_condition_label, ui_theme_font_body(), 0);
    lv_obj_set_style_text_color(s_condition_label, ui_theme_accent_color(), 0);
    lv_label_set_text(s_condition_label, "Yukleniyor...");

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 20, 0);
    lv_obj_set_style_pad_top(row, 8, 0);

    create_stat_card(row, "Hissedilen", &s_feels_label);
    create_stat_card(row, "Nem", &s_humidity_label);
    create_stat_card(row, "Ruzgar", &s_wind_label);

    lv_timer_create(weather_timer_cb, 1000, NULL);
    weather_timer_cb(NULL);
}
