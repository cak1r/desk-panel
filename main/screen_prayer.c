#include "screen_prayer.h"
#include "prayer_times.h"
#include "ui_theme.h"

static lv_obj_t *s_countdown_label;
static lv_obj_t *s_cards[PRAYER_COUNT];
static lv_obj_t *s_time_labels[PRAYER_COUNT];

static const char *DISPLAY_NAMES[PRAYER_COUNT] = { "İmsak", "Güneş", "Oğle", "İkindi", "Akşam", "Yatsı" };

static void highlight_card(int index)
{
    for (int i = 0; i < PRAYER_COUNT; i++) {
        lv_obj_set_style_border_width(s_cards[i], i == index ? 2 : 0, 0);
        lv_obj_set_style_border_color(s_cards[i], ui_theme_accent_color(), 0);
        lv_obj_set_style_text_color(s_time_labels[i], i == index ? ui_theme_accent_color() : lv_color_white(), 0);
    }
}

static void prayer_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    prayer_snapshot_t snap;
    prayer_times_get_snapshot(&snap);

    if (!snap.have_data) {
        lv_label_set_text(s_countdown_label, "--:--:--");
        for (int i = 0; i < PRAYER_COUNT; i++) {
            lv_label_set_text(s_time_labels[i], "--:--");
        }
        highlight_card(-1);
        return;
    }

    for (int i = 0; i < PRAYER_COUNT; i++) {
        lv_label_set_text_fmt(s_time_labels[i], "%02d:%02d", snap.times[i].hour, snap.times[i].minute);
    }
    highlight_card(snap.next_index);

    int32_t remaining = snap.seconds_to_next;
    int h = (int)(remaining / 3600);
    int m = (int)((remaining % 3600) / 60);
    int s = (int)(remaining % 60);
    lv_label_set_text_fmt(s_countdown_label, "%02d:%02d:%02d", h, m, s);
}

void screen_prayer_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, ui_theme_bg_color(), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(parent, 32, 0);
    lv_obj_set_style_pad_row(parent, 0, 0);

    lv_obj_t *countdown_area = lv_obj_create(parent);
    lv_obj_remove_style_all(countdown_area);
    lv_obj_set_size(countdown_area, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(countdown_area, 1);
    lv_obj_set_flex_flow(countdown_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(countdown_area, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_countdown_label = lv_label_create(countdown_area);
    lv_obj_set_style_text_font(s_countdown_label, ui_theme_font_hero(), 0);
    lv_obj_set_style_text_color(s_countdown_label, ui_theme_accent_color(), 0);
    lv_label_set_text(s_countdown_label, "--:--:--");

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < PRAYER_COUNT; i++) {
        lv_obj_t *card = lv_obj_create(row);
        ui_theme_apply_card(card);
        lv_obj_set_size(card, 118, 118);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(card, 4, 0);
        s_cards[i] = card;

        lv_obj_t *name = lv_label_create(card);
        lv_obj_set_style_text_font(name, ui_theme_font_body(), 0);
        lv_label_set_text(name, DISPLAY_NAMES[i]);
        lv_obj_set_style_text_color(name, lv_color_hex(0x8a8f98), 0);

        lv_obj_t *time_lbl = lv_label_create(card);
        lv_obj_set_style_text_font(time_lbl, ui_theme_font_emphasis(), 0);
        lv_label_set_text(time_lbl, "--:--");
        lv_obj_set_style_text_color(time_lbl, lv_color_white(), 0);
        s_time_labels[i] = time_lbl;
    }

    lv_timer_create(prayer_timer_cb, 1000, NULL);
    prayer_timer_cb(NULL);
}
