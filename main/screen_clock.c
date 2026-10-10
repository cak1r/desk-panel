#include "screen_clock.h"
#include "ui_theme.h"

#include <time.h>

static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;

static const char *WEEKDAYS[] = { "Pazar", "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi" };
static const char *MONTHS[] = { "Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran",
                                 "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık" };

static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    lv_label_set_text_fmt(s_time_label, "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
    lv_label_set_text_fmt(s_date_label, "%d %s %s", tm_now.tm_mday, MONTHS[tm_now.tm_mon], WEEKDAYS[tm_now.tm_wday]);
}

void screen_clock_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, ui_theme_bg_color(), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 12, 0);

    s_time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_time_label, ui_theme_font_hero(), 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_letter_space(s_time_label, 2, 0);
    lv_label_set_text(s_time_label, "--:--");

    s_date_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_date_label, ui_theme_font_body(), 0);
    lv_obj_set_style_text_color(s_date_label, ui_theme_accent_color(), 0);
    lv_label_set_text(s_date_label, "-- ------- ---");

    lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);
}
