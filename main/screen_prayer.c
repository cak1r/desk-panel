#include "screen_prayer.h"
#include "ui_format.h"
#include "ui_theme.h"
#include "hadith_state.h"
#include "prayer_times.h"

#include <stdio.h>
#include <time.h>

static const char *WEEKDAYS[] = { "Pazar", "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi" };
static const char *MONTHS[] = { "Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran",
                                "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık" };

/* Dative forms for "<vakit>'e kalan". */
static const char *const DATIVE[PRAYER_COUNT] = {
    "İmsak'a", "Güneş'e", "Öğle'ye", "İkindi'ye", "Akşam'a", "Yatsı'ya",
};

typedef struct {
    lv_obj_t *card;
    lv_obj_t *name;
    lv_obj_t *time;
    lv_obj_t *tag;
} slot_t;

static slot_t s_slots[PRAYER_COUNT];
static lv_obj_t *s_caption;
static lv_obj_t *s_countdown;
static lv_obj_t *s_date;
static lv_obj_t *s_hijri;
static lv_obj_t *s_from;
static lv_obj_t *s_to;
static lv_obj_t *s_progress;
static lv_obj_t *s_hadith_card;
static lv_obj_t *s_hadith_text;
static lv_obj_t *s_hadith_source;
static uint32_t s_hadith_version = UINT32_MAX;

static int prayer_secs(const prayer_time_t *t)
{
    return t->hour * 3600 + t->minute * 60;
}

static void style_slot(slot_t *s, int state) /* 0 past, 1 current, 2 next, 3 later */
{
    uint32_t bg = state == 2 ? UI_COLOR_ACCENT_BG : UI_COLOR_SURFACE;
    uint32_t border = state == 2 ? UI_COLOR_ACCENT : state == 1 ? UI_COLOR_BORDER_STRONG : UI_COLOR_BORDER;
    lv_obj_set_style_bg_color(s->card, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(s->card, lv_color_hex(border), 0);
    lv_obj_set_style_text_color(s->name, lv_color_hex(state == 0 ? UI_COLOR_FAINT : UI_COLOR_TEXT_3), 0);
    lv_obj_set_style_text_color(
        s->time, lv_color_hex(state == 0 ? UI_COLOR_FAINT : state == 2 ? UI_COLOR_ACCENT : UI_COLOR_TEXT), 0);
    lv_obj_set_style_text_color(s->tag, lv_color_hex(state == 2 ? UI_COLOR_ACCENT : UI_COLOR_MUTED), 0);
    lv_label_set_text(s->tag, state == 1 ? "ŞİMDİ" : state == 2 ? "SIRADAKİ" : "");
}

static void refresh_cb(lv_timer_t *timer)
{
    char buf[32];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    lv_label_set_text_fmt(s_date, "%d %s %d %s", tm_now.tm_mday, MONTHS[tm_now.tm_mon], tm_now.tm_year + 1900,
                          WEEKDAYS[tm_now.tm_wday]);

    prayer_snapshot_t snap;
    prayer_times_get_snapshot(&snap);
    if (!snap.have_data || snap.next_index < 0) {
        lv_label_set_text(s_caption, "Vakitler yükleniyor…");
        lv_label_set_text(s_countdown, "");
        return;
    }

    lv_label_set_text(s_hijri, snap.hijri);
    int next = snap.next_index;
    int prev = (next + PRAYER_COUNT - 1) % PRAYER_COUNT;

    lv_label_set_text_fmt(s_caption, "%s kalan", DATIVE[next]);
    ui_format_countdown_long(snap.seconds_to_next, buf, sizeof(buf));
    lv_label_set_text(s_countdown, buf);

    /* Progress from the previous prayer to the next one; before İmsak the
     * previous one is yesterday's Yatsı. */
    int now_secs = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;
    int prev_secs = prayer_secs(&snap.times[prev]);
    if (prev_secs > now_secs) {
        prev_secs -= 24 * 3600;
    }
    int next_secs = now_secs + snap.seconds_to_next;
    int span = next_secs - prev_secs;
    lv_bar_set_value(s_progress, span > 0 ? (now_secs - prev_secs) * 100 / span : 0, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_from, "%s %02d:%02d", snap.times[prev].name, snap.times[prev].hour, snap.times[prev].minute);
    lv_label_set_text_fmt(s_to, "%s %02d:%02d", snap.times[next].name, snap.times[next].hour, snap.times[next].minute);

    /* next == 0 means either after Yatsı (all of today is past) or before
     * İmsak (all of today is still ahead). */
    bool after_isha = now_secs >= prayer_secs(&snap.times[PRAYER_COUNT - 1]);
    for (int i = 0; i < PRAYER_COUNT; i++) {
        lv_label_set_text(s_slots[i].name, snap.times[i].name);
        lv_label_set_text_fmt(s_slots[i].time, "%02d:%02d", snap.times[i].hour, snap.times[i].minute);
        int state;
        if (i == next) {
            state = 2;
        } else if (next != 0) {
            state = i < prev ? 0 : i == prev ? 1 : 3;
        } else if (after_isha) {
            state = i == PRAYER_COUNT - 1 ? 1 : 0;
        } else {
            state = 3;
        }
        style_slot(&s_slots[i], state);
    }

    hadith_snapshot_t h;
    hadith_state_get_snapshot(&h);
    if (h.version != s_hadith_version) {
        s_hadith_version = h.version;
        if (h.have_data) {
            lv_label_set_text(s_hadith_text, h.text);
            if (h.grade[0]) {
                lv_label_set_text_fmt(s_hadith_source, "%s · %s", h.source, h.grade);
            } else {
                lv_label_set_text(s_hadith_source, h.source);
            }
            lv_obj_clear_flag(s_hadith_card, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_hadith_card, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void screen_prayer_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 14, 0);

    /* Countdown + date */
    lv_obj_t *hero = ui_box(parent);
    lv_obj_set_size(hero, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

    lv_obj_t *left = ui_box(hero);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    s_caption = ui_label(left, UI_FONT_15, UI_COLOR_MUTED, "");
    s_countdown = ui_label(left, UI_FONT_NUM_60, UI_COLOR_ACCENT, "");

    lv_obj_t *right = ui_box(hero);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(right, 6, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(right, 2, 0);
    s_date = ui_label(right, UI_FONT_18, UI_COLOR_TEXT, "");
    s_hijri = ui_label(right, UI_FONT_15, UI_COLOR_MUTED, "");

    /* Previous -> next progress */
    lv_obj_t *prog = ui_box(parent);
    lv_obj_set_size(prog, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(prog, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(prog, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(prog, 10, 0);
    s_from = ui_label(prog, UI_FONT_13, UI_COLOR_MUTED, "");
    s_progress = ui_progress(prog, 10, 4, UI_COLOR_ACCENT);
    lv_obj_set_flex_grow(s_progress, 1);
    s_to = ui_label(prog, UI_FONT_13, UI_COLOR_MUTED, "");

    /* Six times */
    static const lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                       LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const lv_coord_t rows[] = { LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST };
    lv_obj_t *grid = ui_box(parent);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(grid, 10, 0);
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    for (int i = 0; i < PRAYER_COUNT; i++) {
        slot_t *s = &s_slots[i];
        s->card = ui_box(grid);
        ui_theme_apply_surface(s->card);
        lv_obj_set_style_radius(s->card, 16, 0);
        lv_obj_set_style_pad_all(s->card, 12, 0);
        lv_obj_set_grid_cell(s->card, LV_GRID_ALIGN_STRETCH, i, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_set_height(s->card, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(s->card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(s->card, 4, 0);
        s->name = ui_label(s->card, UI_FONT_15, UI_COLOR_TEXT_3, "");
        s->time = ui_label(s->card, UI_FONT_26, UI_COLOR_TEXT, "--:--");
        s->tag = ui_label(s->card, UI_FONT_13, UI_COLOR_MUTED, "");
    }

    /* Hadith of the day (hidden until the companion sends one) */
    s_hadith_card = ui_box(parent);
    ui_theme_apply_surface(s_hadith_card);
    lv_obj_set_width(s_hadith_card, LV_PCT(100));
    lv_obj_set_flex_grow(s_hadith_card, 1);
    lv_obj_set_style_pad_hor(s_hadith_card, 20, 0);
    lv_obj_set_style_pad_ver(s_hadith_card, 14, 0);
    lv_obj_set_flex_flow(s_hadith_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_hadith_card, 8, 0);
    lv_obj_t *head = ui_box(s_hadith_card);
    lv_obj_set_size(head, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(head, 8, 0);
    ui_label(head, UI_FONT_ICON_18, UI_COLOR_MUTED, UI_ICON_BOOK);
    ui_label(head, UI_FONT_13, UI_COLOR_MUTED, "GÜNÜN HADİSİ");
    s_hadith_text = ui_label(s_hadith_card, UI_FONT_18, 0xdfe1e5, "");
    lv_label_set_long_mode(s_hadith_text, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_hadith_text, LV_PCT(100));
    lv_obj_set_flex_grow(s_hadith_text, 1);
    lv_obj_set_style_text_line_space(s_hadith_text, 6, 0);
    s_hadith_source = ui_label(s_hadith_card, UI_FONT_13, UI_COLOR_MUTED, "");
    lv_obj_add_flag(s_hadith_card, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(refresh_cb, 1000, NULL);
    refresh_cb(NULL);
}
