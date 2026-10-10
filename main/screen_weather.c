#include "screen_weather.h"
#include "ui_theme.h"
#include "weather_state.h"

#include <stdio.h>

static const char *WEEKDAYS_SHORT[] = { "Paz", "Pzt", "Sal", "Çar", "Per", "Cum", "Cmt" };

typedef struct {
    lv_obj_t *row;
    lv_obj_t *day;
    lv_obj_t *icon;
    lv_obj_t *rain;
    lv_obj_t *rain_bar;
    lv_obj_t *min;
    lv_obj_t *range_fill;
    lv_obj_t *max;
} day_row_t;

typedef struct {
    lv_obj_t *pct;
    lv_obj_t *bar;
    lv_obj_t *hour;
} hour_col_t;

static lv_obj_t *s_temp;
static lv_obj_t *s_icon;
static lv_obj_t *s_cond;
static lv_obj_t *s_details;
static lv_obj_t *s_hours_box;
static hour_col_t s_hours[WEATHER_MAX_HOURS];
static day_row_t s_days[WEATHER_MAX_DAYS];

#define HOUR_BAR_MAX 52

static void icon_for(int code, const char **icon, uint32_t *color)
{
    *color = UI_COLOR_TEXT_2;
    if (code <= 1) {
        *icon = UI_ICON_SUN;
        *color = UI_COLOR_WARM;
    } else if (code <= 3) {
        *icon = UI_ICON_CLOUD;
    } else if (code == 45 || code == 48) {
        *icon = UI_ICON_CLOUD_FOG;
    } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        *icon = UI_ICON_CLOUD_SNOW;
    } else if (code >= 95) {
        *icon = UI_ICON_CLOUD_STORM;
        *color = UI_COLOR_RAIN;
    } else if (code >= 51) {
        *icon = UI_ICON_CLOUD_RAIN;
        *color = UI_COLOR_RAIN;
    } else {
        *icon = UI_ICON_CLOUD;
    }
}

static void refresh_cb(lv_timer_t *timer)
{
    weather_snapshot_t snap;
    weather_state_get_snapshot(&snap);
    if (!snap.have_data) {
        lv_label_set_text(s_cond, "Hava durumu yükleniyor…");
        return;
    }

    const char *icon;
    uint32_t color;
    lv_label_set_text_fmt(s_temp, "%d°", snap.temperature_c);
    icon_for(snap.weather_code, &icon, &color);
    lv_label_set_text(s_icon, icon);
    lv_obj_set_style_text_color(s_icon, lv_color_hex(color), 0);
    lv_label_set_text(s_cond, weather_state_condition_text(snap.weather_code));
    lv_label_set_text_fmt(s_details, "Hissedilen %d° · Nem %%%d · Rüzgâr %d km/sa", snap.feels_like_c,
                          snap.humidity_percent, snap.wind_kmh);

    for (int i = 0; i < WEATHER_MAX_HOURS; i++) {
        hour_col_t *h = &s_hours[i];
        if (i >= snap.hour_count) {
            lv_obj_add_flag(lv_obj_get_parent(h->bar), LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        int p = snap.hours[i].rain_percent;
        lv_label_set_text_fmt(h->pct, "%%%d", p);
        lv_obj_set_height(h->bar, 4 + p * (HOUR_BAR_MAX - 4) / 100);
        lv_obj_set_style_bg_opa(h->bar, (lv_opa_t)(115 + p * 140 / 100), 0);
        lv_label_set_text_fmt(h->hour, "%02d", snap.hours[i].hour);
        lv_obj_clear_flag(lv_obj_get_parent(h->bar), LV_OBJ_FLAG_HIDDEN);
    }
    if (snap.hour_count == 0) {
        lv_obj_add_flag(s_hours_box, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_hours_box, LV_OBJ_FLAG_HIDDEN);
    }

    /* One temperature scale across all days so the bars compare. */
    int lo = 100, hi = -100;
    for (int i = 0; i < snap.day_count; i++) {
        lo = snap.days[i].min_c < lo ? snap.days[i].min_c : lo;
        hi = snap.days[i].max_c > hi ? snap.days[i].max_c : hi;
    }
    int span = hi > lo ? hi - lo : 1;

    for (int i = 0; i < WEATHER_MAX_DAYS; i++) {
        day_row_t *r = &s_days[i];
        if (i >= snap.day_count) {
            lv_obj_add_flag(r->row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const weather_day_t *d = &snap.days[i];
        if (i == 0) {
            lv_label_set_text(r->day, "Bugün");
        } else {
            lv_label_set_text_fmt(r->day, "%s %d", WEEKDAYS_SHORT[d->wday], d->mday);
        }
        icon_for(d->weather_code, &icon, &color);
        lv_label_set_text(r->icon, icon);
        lv_obj_set_style_text_color(r->icon, lv_color_hex(color), 0);
        lv_label_set_text_fmt(r->rain, "%%%d", d->rain_percent);
        lv_obj_set_style_text_color(r->rain, lv_color_hex(d->rain_percent >= 50 ? UI_COLOR_RAIN : UI_COLOR_MUTED), 0);
        lv_obj_set_width(r->rain_bar, lv_pct(d->rain_percent));
        lv_label_set_text_fmt(r->min, "%d°", d->min_c);
        lv_label_set_text_fmt(r->max, "%d°", d->max_c);
        int width_pct = (d->max_c - d->min_c) * 100 / span;
        lv_obj_set_x(r->range_fill, lv_pct((d->min_c - lo) * 100 / span));
        lv_obj_set_width(r->range_fill, lv_pct(width_pct > 2 ? width_pct : 2)); /* keep a visible sliver */
        lv_obj_clear_flag(r->row, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *track(lv_obj_t *parent, int height)
{
    lv_obj_t *t = ui_box(parent);
    lv_obj_set_height(t, height);
    lv_obj_set_style_radius(t, height / 2, 0);
    lv_obj_set_style_bg_color(t, lv_color_hex(UI_COLOR_SURFACE_3), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    return t;
}

static lv_obj_t *fill(lv_obj_t *track_obj, uint32_t color)
{
    lv_obj_t *f = ui_box(track_obj);
    lv_obj_set_size(f, 0, LV_PCT(100));
    lv_obj_set_style_radius(f, lv_obj_get_style_radius(track_obj, 0), 0);
    lv_obj_set_style_bg_color(f, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    return f;
}

static void build_now_card(lv_obj_t *parent)
{
    lv_obj_t *card = ui_box(parent);
    ui_theme_apply_surface(card);
    lv_obj_set_size(card, 250, LV_PCT(100));
    lv_obj_set_style_pad_all(card, 18, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);

    lv_obj_t *top = ui_box(card);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_temp = ui_label(top, UI_FONT_NUM_60, UI_COLOR_TEXT, "--°");
    s_icon = ui_label(top, UI_FONT_ICON_36, UI_COLOR_TEXT_2, UI_ICON_CLOUD);

    lv_obj_t *text = ui_box(card);
    lv_obj_set_size(text, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 4, 0);
    s_cond = ui_label(text, UI_FONT_18, UI_COLOR_TEXT, "");
    s_details = ui_label(text, UI_FONT_13, UI_COLOR_MUTED, "");
    lv_label_set_long_mode(s_details, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_details, LV_PCT(100));

    lv_obj_t *spacer = ui_box(card);
    lv_obj_set_width(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);

    s_hours_box = ui_box(card);
    lv_obj_set_size(s_hours_box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_hours_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_hours_box, 8, 0);
    ui_label(s_hours_box, UI_FONT_13, UI_COLOR_MUTED, "YAĞIŞ İHTİMALİ · SAATLİK");

    lv_obj_t *bars = ui_box(s_hours_box);
    lv_obj_set_size(bars, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bars, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bars, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    for (int i = 0; i < WEATHER_MAX_HOURS; i++) {
        lv_obj_t *col = ui_box(bars);
        lv_obj_set_size(col, 30, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(col, 4, 0);
        s_hours[i].pct = ui_label(col, UI_FONT_13, UI_COLOR_TEXT_3, "");
        s_hours[i].bar = ui_box(col);
        lv_obj_set_size(s_hours[i].bar, LV_PCT(100), 4);
        lv_obj_set_style_radius(s_hours[i].bar, 4, 0);
        lv_obj_set_style_bg_color(s_hours[i].bar, lv_color_hex(UI_COLOR_RAIN), 0);
        lv_obj_set_style_bg_opa(s_hours[i].bar, LV_OPA_COVER, 0);
        s_hours[i].hour = ui_label(col, UI_FONT_13, UI_COLOR_MUTED, "");
        lv_obj_add_flag(col, LV_OBJ_FLAG_HIDDEN);
    }
}

static void build_day_list(lv_obj_t *parent)
{
    lv_obj_t *list = ui_box(parent);
    lv_obj_set_height(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 2, 0);

    for (int i = 0; i < WEATHER_MAX_DAYS; i++) {
        day_row_t *r = &s_days[i];
        r->row = ui_box(list);
        lv_obj_set_width(r->row, LV_PCT(100));
        lv_obj_set_flex_grow(r->row, 1);
        lv_obj_set_style_pad_hor(r->row, 14, 0);
        lv_obj_set_style_radius(r->row, 12, 0);
        lv_obj_set_flex_flow(r->row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r->row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r->row, 12, 0);
        if (i == 0) {
            ui_theme_apply_surface(r->row);
            lv_obj_set_style_radius(r->row, 12, 0);
            lv_obj_set_style_pad_ver(r->row, 0, 0);
        }

        r->day = ui_label(r->row, UI_FONT_15, UI_COLOR_TEXT, "");
        lv_obj_set_width(r->day, 64);
        r->icon = ui_label(r->row, UI_FONT_ICON_18, UI_COLOR_TEXT_2, "");
        lv_obj_set_width(r->icon, 22);

        lv_obj_t *rain = ui_box(r->row);
        lv_obj_set_size(rain, 96, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(rain, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(rain, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(rain, 6, 0);
        ui_label(rain, UI_FONT_ICON_18, UI_COLOR_RAIN, UI_ICON_DROPLET);
        r->rain = ui_label(rain, UI_FONT_13, UI_COLOR_MUTED, "");
        lv_obj_set_width(r->rain, 34);
        lv_obj_t *rt = track(rain, 4);
        lv_obj_set_flex_grow(rt, 1);
        r->rain_bar = fill(rt, UI_COLOR_RAIN);

        r->min = ui_label(r->row, UI_FONT_15, UI_COLOR_MUTED, "");
        lv_obj_set_width(r->min, 34);
        lv_obj_set_style_text_align(r->min, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_t *range = track(r->row, 6);
        lv_obj_set_flex_grow(range, 1);
        r->range_fill = fill(range, UI_COLOR_WARM);
        r->max = ui_label(r->row, UI_FONT_15, UI_COLOR_TEXT, "");
        lv_obj_set_width(r->max, 34);

        lv_obj_add_flag(r->row, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen_weather_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(parent, 16, 0);

    build_now_card(parent);
    build_day_list(parent);

    lv_timer_create(refresh_cb, 5000, NULL);
    refresh_cb(NULL);
}
