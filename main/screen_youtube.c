#include "screen_youtube.h"
#include "ui.h"
#include "ui_art.h"
#include "ui_format.h"
#include "ui_theme.h"
#include "hid_actions.h"
#include "media_state.h"

#include <stdio.h>

typedef struct {
    const char *icon;
    const char *label;
    const char *hint;  /* shown on the key cap */
    const char *keys;  /* typed via the Turkish Q mapping, e.g. "N" = Shift+N */
    bool primary;
} yt_key_t;

/* YouTube's built-in player shortcuts. */
static const yt_key_t KEYS[] = {
    { UI_ICON_SEEK_BACK_10, "10 sn geri", "J", "j", false },
    { UI_ICON_PLAYER_PAUSE, "Oynat / duraklat", "K", "k", true },
    { UI_ICON_SEEK_FORWARD_10, "10 sn ileri", "L", "l", false },
    { UI_ICON_VOLUME_OFF, "Sessiz", "M", "m", false },
    { UI_ICON_MAXIMIZE, "Tam ekran", "F", "f", false },
    { UI_ICON_THEATER, "Sinema", "T", "t", false },
    { UI_ICON_CAPTIONS, "Altyazı", "C", "c", false },
    { UI_ICON_MINIPLAYER, "Mini oynatıcı", "I", "i", false },
    { UI_ICON_GAUGE, "Yavaşlat", "<", "<", false },
    { UI_ICON_GAUGE, "Hızlandır", ">", ">", false },
    { UI_ICON_PLAYER_TRACK_PREV, "Önceki", "Shift P", "P", false },
    { UI_ICON_PLAYER_TRACK_NEXT, "Sonraki", "Shift N", "N", false },
};
#define KEY_COUNT (sizeof(KEYS) / sizeof(KEYS[0]))

static lv_obj_t *s_title;
static lv_obj_t *s_channel;
static lv_obj_t *s_progress;
static lv_obj_t *s_elapsed;
static lv_obj_t *s_total;

static void key_cb(lv_event_t *e)
{
    const yt_key_t *k = lv_event_get_user_data(e);
    hid_actions_text(k->keys);
}

static void refresh_cb(lv_timer_t *timer)
{
    media_snapshot_t snap;
    media_state_get_snapshot(&snap);
    char buf[16];

    if (!snap.active) {
        lv_label_set_text(s_title, "Oynatılan video yok");
        lv_label_set_text(s_channel, "");
        lv_bar_set_value(s_progress, 0, LV_ANIM_OFF);
        lv_label_set_text(s_elapsed, "");
        lv_label_set_text(s_total, "");
        return;
    }
    lv_label_set_text(s_title, snap.title);
    lv_label_set_text(s_channel, snap.artist);
    lv_bar_set_value(s_progress, snap.duration_s > 0 ? (int32_t)(snap.position_s * 100 / snap.duration_s) : 0,
                     LV_ANIM_OFF);
    ui_format_track_time(snap.position_s, buf, sizeof(buf));
    lv_label_set_text(s_elapsed, buf);
    ui_format_track_time(snap.duration_s, buf, sizeof(buf));
    lv_label_set_text(s_total, snap.duration_s > 0 ? buf : "");
}

static void build_key(lv_obj_t *grid, int index)
{
    const yt_key_t *k = &KEYS[index];
    lv_obj_t *key = lv_obj_create(grid);
    ui_theme_apply_key(key);
    lv_obj_set_style_radius(key, 16, 0);
    lv_obj_set_style_pad_all(key, 12, 0);
    if (k->primary) {
        lv_obj_set_style_bg_color(key, lv_color_hex(UI_COLOR_ACCENT_BG), 0);
        lv_obj_set_style_border_color(key, lv_color_hex(UI_COLOR_ACCENT), 0);
    }
    lv_obj_set_grid_cell(key, LV_GRID_ALIGN_STRETCH, index % 4, 1, LV_GRID_ALIGN_STRETCH, index / 4, 1);
    lv_obj_set_flex_flow(key, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(key, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_event_cb(key, key_cb, LV_EVENT_CLICKED, (void *)k);

    ui_label(key, UI_FONT_ICON_24, k->primary ? UI_COLOR_ACCENT : UI_COLOR_TEXT, k->icon);

    lv_obj_t *row = ui_box(key);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *label = ui_label(row, UI_FONT_13, k->primary ? UI_COLOR_TEXT : UI_COLOR_TEXT_2, k->label);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(label, 1);

    lv_obj_t *cap = ui_box(row);
    lv_obj_set_size(cap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(cap, 6, 0);
    lv_obj_set_style_pad_ver(cap, 1, 0);
    lv_obj_set_style_radius(cap, 5, 0);
    lv_obj_set_style_border_width(cap, 1, 0);
    lv_obj_set_style_border_color(cap, lv_color_hex(0x2f353e), 0);
    ui_label(cap, UI_FONT_13, UI_COLOR_MUTED, k->hint);
}

void screen_youtube_create(lv_obj_t *parent)
{
    lv_label_set_text(ui_page_subtitle(UI_PAGE_YOUTUBE), "kısayollar öndeki YouTube sekmesine gider");
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(parent, 16, 0);
    lv_obj_set_style_pad_top(parent, 6, 0);

    /* Video card */
    lv_obj_t *card = ui_box(parent);
    ui_theme_apply_surface(card);
    lv_obj_set_size(card, 236, LV_PCT(100));
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);

    ui_art_create(card, 120);
    s_title = ui_label(card, UI_FONT_15, UI_COLOR_TEXT, "");
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_title, LV_PCT(100));
    lv_obj_set_style_max_height(s_title, 60, 0);
    s_channel = ui_label(card, UI_FONT_13, UI_COLOR_MUTED, "");
    lv_label_set_long_mode(s_channel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_channel, LV_PCT(100));

    lv_obj_t *spacer = ui_box(card);
    lv_obj_set_width(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);
    s_progress = ui_progress(card, LV_PCT(100), 4, UI_COLOR_ACCENT);
    lv_obj_t *times = ui_box(card);
    lv_obj_set_size(times, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(times, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(times, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_elapsed = ui_label(times, UI_FONT_13, UI_COLOR_MUTED, "");
    s_total = ui_label(times, UI_FONT_13, UI_COLOR_MUTED, "");

    /* Shortcut keys */
    static const lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                       LV_GRID_TEMPLATE_LAST };
    static const lv_coord_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_t *grid = ui_box(parent);
    lv_obj_set_height(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    for (int i = 0; i < (int)KEY_COUNT; i++) {
        build_key(grid, i);
    }

    lv_timer_create(refresh_cb, 1000, NULL);
    refresh_cb(NULL);
}
