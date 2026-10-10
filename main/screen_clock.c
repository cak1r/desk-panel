#include "screen_clock.h"
#include "ui_art.h"
#include "ui_format.h"
#include "ui_theme.h"
#include "media_state.h"
#include "prayer_times.h"
#include "weather_state.h"
#include "whatsapp_state.h"

#include <stdio.h>
#include <time.h>

static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_temp_label;
static lv_obj_t *s_cond_label;
static lv_obj_t *s_media_chip;
static lv_obj_t *s_media_title;
static lv_obj_t *s_media_artist;
static lv_obj_t *s_prayer_chip;
static lv_obj_t *s_prayer_caption;
static lv_obj_t *s_prayer_value;
static lv_obj_t *s_wa_chip;
static lv_obj_t *s_wa_value;

static const char *WEEKDAYS[] = { "Pazar", "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi" };
static const char *MONTHS[] = { "Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran",
                                "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık" };

/* "İkindi" -> "İkindi'ye" style dative suffix for the countdown caption. */
static const char *prayer_dative(int index)
{
    static const char *const NAMES[PRAYER_COUNT] = {
        "İmsak'a", "Güneş'e", "Öğle'ye", "İkindi'ye", "Akşam'a", "Yatsı'ya",
    };
    return (index >= 0 && index < PRAYER_COUNT) ? NAMES[index] : "";
}

static lv_obj_t *chip(lv_obj_t *parent)
{
    lv_obj_t *c = ui_box(parent);
    ui_theme_apply_surface(c);
    lv_obj_set_style_radius(c, 16, 0);
    lv_obj_set_style_pad_all(c, 12, 0);
    lv_obj_set_height(c, 78);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c, 12, 0);
    return c;
}

static lv_obj_t *column(lv_obj_t *parent)
{
    lv_obj_t *col = ui_box(parent);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);
    return col;
}

static void clock_timer_cb(lv_timer_t *timer)
{
    char buf[32];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    lv_label_set_text_fmt(s_time_label, "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
    lv_label_set_text_fmt(s_date_label, "%d %s %s", tm_now.tm_mday, MONTHS[tm_now.tm_mon], WEEKDAYS[tm_now.tm_wday]);

    weather_snapshot_t weather;
    weather_state_get_snapshot(&weather);
    if (weather.have_data) {
        lv_label_set_text_fmt(s_temp_label, "%d°", weather.temperature_c);
        lv_label_set_text(s_cond_label, weather_state_condition_text(weather.weather_code));
    }

    /* Glance chips only show when they have something to say. */
    media_snapshot_t media;
    media_state_get_snapshot(&media);
    if (media.active && media.title[0]) {
        lv_label_set_text(s_media_title, media.title);
        lv_label_set_text_fmt(s_media_artist, "%s%s%s", media.artist, media.artist[0] ? " · " : "", media.source);
        lv_obj_clear_flag(s_media_chip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_media_chip, LV_OBJ_FLAG_HIDDEN);
    }

    prayer_snapshot_t prayer;
    prayer_times_get_snapshot(&prayer);
    if (prayer.have_data && prayer.next_index >= 0) {
        lv_label_set_text(s_prayer_caption, prayer_dative(prayer.next_index));
        int32_t s = prayer.seconds_to_next;
        if (s >= 3600) {
            lv_label_set_text_fmt(s_prayer_value, "%ld sa %ld dk", (long)(s / 3600), (long)((s % 3600) / 60));
        } else {
            ui_format_countdown_short(s, buf, sizeof(buf));
            lv_label_set_text_fmt(s_prayer_value, "%s dk", buf);
        }
        lv_obj_clear_flag(s_prayer_chip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_prayer_chip, LV_OBJ_FLAG_HIDDEN);
    }

    whatsapp_snapshot_t wa;
    whatsapp_state_get_snapshot(&wa);
    if (wa.count > 0) {
        lv_label_set_text_fmt(s_wa_value, "%d", wa.count);
        lv_obj_clear_flag(s_wa_chip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_wa_chip, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen_clock_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_top(parent, 36, 0);
    lv_obj_set_style_pad_hor(parent, 44, 0);
    lv_obj_set_style_pad_bottom(parent, 28, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    /* Top: time + date on the left, weather on the right. */
    lv_obj_t *top = ui_box(parent);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *left = ui_box(top);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(left, 6, 0);
    s_time_label = ui_label(left, UI_FONT_NUM_163, UI_COLOR_TEXT, "--:--");
    lv_obj_set_style_text_letter_space(s_time_label, -4, 0);
    s_date_label = ui_label(left, UI_FONT_20_LIGHT, UI_COLOR_TEXT_3, "");

    lv_obj_t *right = ui_box(top);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(right, 18, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(right, 4, 0);
    s_temp_label = ui_label(right, UI_FONT_32, UI_COLOR_TEXT, "");
    s_cond_label = ui_label(right, UI_FONT_13, UI_COLOR_MUTED, "");

    /* Glance chips. */
    lv_obj_t *chips = ui_box(parent);
    lv_obj_set_size(chips, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(chips, 12, 0);

    s_media_chip = chip(chips);
    lv_obj_set_flex_grow(s_media_chip, 16);
    ui_art_create(s_media_chip, 52);
    lv_obj_t *col = column(s_media_chip);
    s_media_title = ui_label(col, UI_FONT_15, UI_COLOR_TEXT, "");
    s_media_artist = ui_label(col, UI_FONT_13, UI_COLOR_MUTED, "");
    lv_label_set_long_mode(s_media_title, LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(s_media_artist, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_media_title, LV_PCT(100));
    lv_obj_set_width(s_media_artist, LV_PCT(100));

    s_prayer_chip = chip(chips);
    lv_obj_set_flex_grow(s_prayer_chip, 10);
    col = column(s_prayer_chip);
    s_prayer_caption = ui_label(col, UI_FONT_13, UI_COLOR_MUTED, "");
    s_prayer_value = ui_label(col, UI_FONT_18, UI_COLOR_ACCENT, "");

    s_wa_chip = chip(chips);
    lv_obj_set_flex_grow(s_wa_chip, 10);
    ui_icon_chip(s_wa_chip, 40, UI_TINT_WHATSAPP_BG, UI_TINT_WHATSAPP_FG, UI_FONT_ICON_24, UI_ICON_WHATSAPP);
    col = column(s_wa_chip);
    s_wa_value = ui_label(col, UI_FONT_18, UI_COLOR_TEXT, "");
    ui_label(col, UI_FONT_13, UI_COLOR_MUTED, "yeni bildirim");

    lv_obj_t *hint = ui_box(parent);
    lv_obj_set_size(hint, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hint, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(hint, 6, 0);
    ui_label(hint, UI_FONT_ICON_18, UI_COLOR_FAINT, UI_ICON_CHEVRON_UP);
    ui_label(hint, UI_FONT_13, UI_COLOR_FAINT, "Deck için dokun");

    lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);
}
