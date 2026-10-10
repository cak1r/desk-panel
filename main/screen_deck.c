#include "screen_deck.h"
#include "ui.h"
#include "ui_art.h"
#include "ui_theme.h"
#include "ui_format.h"
#include "hid_actions.h"
#include "audio_state.h"
#include "claude_usage.h"
#include "media_state.h"
#include "mixer_state.h"
#include "pc_health_state.h"
#include "prayer_times.h"
#include "weather_state.h"
#include "whatsapp_state.h"

#include "class/hid/hid.h"

#include <stdio.h>
#include <time.h>

#define GRID_COLS 5
#define GRID_ROWS 3

typedef enum {
    ACT_PAGE,
    ACT_KEY,     /* modifier + keycode chord */
    ACT_LAUNCH,  /* Win+R command */
    ACT_MIC_TOGGLE,
} action_kind_t;

typedef struct {
    action_kind_t kind;
    int page;
    uint8_t modifier;
    uint8_t keycode;
    const char *command;
} key_action_t;

/* Live widgets refreshed by the 1 s timer. */
static struct {
    lv_obj_t *clock;
    lv_obj_t *pc_dot;
    lv_obj_t *pc_label;

    lv_obj_t *media_title;
    lv_obj_t *media_artist;
    lv_obj_t *media_source;
    lv_obj_t *media_progress;
    lv_obj_t *media_play_icon;

    lv_obj_t *wa_badge;
    lv_obj_t *wa_badge_label;

    lv_obj_t *mic_key;
    lv_obj_t *mic_chip;
    lv_obj_t *mic_icon;
    lv_obj_t *mic_label;
    bool mic_muted;

    lv_obj_t *claude_value;
    lv_obj_t *prayer_caption;
    lv_obj_t *prayer_value;
    lv_obj_t *prayer_sub;
    lv_obj_t *weather_value;
    lv_obj_t *weather_sub;
    lv_obj_t *volume_value;
    lv_obj_t *volume_bar;
    lv_obj_t *cpu_value;
    lv_obj_t *cpu_bar;
} w;

/* ---------------------------------------------------------------- actions */

static void key_clicked_cb(lv_event_t *e)
{
    const key_action_t *a = lv_event_get_user_data(e);
    switch (a->kind) {
    case ACT_PAGE:
        ui_open_page((ui_page_t)a->page);
        break;
    case ACT_KEY:
        hid_actions_key(a->modifier, a->keycode);
        break;
    case ACT_LAUNCH:
        hid_actions_launch(a->command);
        break;
    case ACT_MIC_TOGGLE:
        mixer_state_set_mute(MIXER_TARGET_MIC, !w.mic_muted);
        break;
    }
}

static void media_play_cb(lv_event_t *e)
{
    media_state_command("play_pause");
}

static const key_action_t A_NOWPLAYING = { .kind = ACT_PAGE, .page = UI_PAGE_NOWPLAYING };
static const key_action_t A_WHATSAPP = { .kind = ACT_PAGE, .page = UI_PAGE_WHATSAPP };
static const key_action_t A_YOUTUBE = { .kind = ACT_PAGE, .page = UI_PAGE_YOUTUBE };
static const key_action_t A_MIC = { .kind = ACT_MIC_TOGGLE };
/* Discord's default "Toggle Deafen" keybind. */
static const key_action_t A_DISCORD_DEAFEN = { .kind = ACT_KEY,
                                               .modifier = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT,
                                               .keycode = HID_KEY_D };
/* Windows Snipping Tool region capture. */
static const key_action_t A_SCREENSHOT = { .kind = ACT_KEY,
                                           .modifier = KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_LEFTSHIFT,
                                           .keycode = HID_KEY_S };
static const key_action_t A_VSCODE = { .kind = ACT_LAUNCH, .command = "code" };
static const key_action_t A_TERMINAL = { .kind = ACT_LAUNCH, .command = "wt" };
static const key_action_t A_LOCK = { .kind = ACT_KEY, .modifier = KEYBOARD_MODIFIER_LEFTGUI, .keycode = HID_KEY_L };
static const key_action_t A_CLAUDE = { .kind = ACT_PAGE, .page = UI_PAGE_CLAUDE };
static const key_action_t A_PRAYER = { .kind = ACT_PAGE, .page = UI_PAGE_PRAYER };
static const key_action_t A_WEATHER = { .kind = ACT_PAGE, .page = UI_PAGE_WEATHER };
static const key_action_t A_MIXER = { .kind = ACT_PAGE, .page = UI_PAGE_MIXER };
static const key_action_t A_HEALTH = { .kind = ACT_PAGE, .page = UI_PAGE_HEALTH };

/* ------------------------------------------------------------------- keys */

static lv_obj_t *key_base(lv_obj_t *grid, int col, int row, int col_span, const key_action_t *action)
{
    lv_obj_t *k = lv_obj_create(grid);
    ui_theme_apply_key(k);
    lv_obj_set_grid_cell(k, LV_GRID_ALIGN_STRETCH, col, col_span, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_add_event_cb(k, key_clicked_cb, LV_EVENT_CLICKED, (void *)action);
    return k;
}

/* Icon chip on top, label at the bottom. */
static lv_obj_t *key_app(lv_obj_t *grid, int col, int row, uint32_t tint_bg, uint32_t tint_fg, const char *icon,
                         const char *label, const key_action_t *action)
{
    lv_obj_t *k = key_base(grid, col, row, 1, action);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    ui_icon_chip(k, 44, tint_bg, tint_fg, UI_FONT_ICON_24, icon);
    lv_obj_t *l = ui_label(k, UI_FONT_15, UI_COLOR_TEXT_2, label);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));
    return k;
}

/* Small caption on top, big value, sub line (or a bar) below. */
static lv_obj_t *key_live(lv_obj_t *grid, int col, int row, const char *icon, const char *caption,
                          const key_action_t *action, lv_obj_t **caption_out, lv_obj_t **value_out,
                          lv_obj_t **sub_out)
{
    lv_obj_t *k = key_base(grid, col, row, 1, action);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *head = ui_box(k);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, 6, 0);
    ui_label(head, UI_FONT_ICON_18, UI_COLOR_MUTED, icon);
    lv_obj_t *cap = ui_label(head, UI_FONT_13, UI_COLOR_MUTED, caption);
    if (caption_out) {
        *caption_out = cap;
    }

    lv_obj_t *body = ui_box(k);
    lv_obj_set_size(body, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 4, 0);
    *value_out = ui_label(body, UI_FONT_26, UI_COLOR_TEXT, "—");
    if (sub_out) {
        *sub_out = ui_label(body, UI_FONT_13, UI_COLOR_MUTED, "");
        lv_label_set_long_mode(*sub_out, LV_LABEL_LONG_DOT);
        lv_obj_set_width(*sub_out, LV_PCT(100));
    }
    return body;
}

static void build_media_key(lv_obj_t *grid)
{
    lv_obj_t *k = key_base(grid, 0, 0, 2, &A_NOWPLAYING);
    lv_obj_set_style_pad_all(k, 12, 0);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(k, 14, 0);

    ui_art_create(k, 92);

    lv_obj_t *col = ui_box(k);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 3, 0);

    w.media_source = ui_label(col, UI_FONT_13, UI_TINT_YOUTUBE_FG, "");
    w.media_title = ui_label(col, UI_FONT_18, UI_COLOR_TEXT, "Çalan bir şey yok");
    w.media_artist = ui_label(col, UI_FONT_13, UI_COLOR_MUTED, "");
    lv_obj_t *labels[] = { w.media_source, w.media_title, w.media_artist };
    for (int i = 0; i < 3; i++) {
        lv_label_set_long_mode(labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(labels[i], LV_PCT(100));
    }
    ui_spacer(col, 1, 5);
    w.media_progress = ui_progress(col, LV_PCT(100), 4, UI_COLOR_ACCENT);

    lv_obj_t *play = lv_obj_create(k);
    lv_obj_remove_style_all(play);
    lv_obj_set_size(play, 48, 48);
    lv_obj_add_flag(play, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(play, 24, 0);
    lv_obj_set_style_bg_color(play, lv_color_hex(UI_COLOR_SURFACE_3), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(play, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(play, media_play_cb, LV_EVENT_CLICKED, NULL);
    w.media_play_icon = ui_label(play, UI_FONT_ICON_24, UI_COLOR_TEXT, UI_ICON_PLAYER_PLAY);
    lv_obj_center(w.media_play_icon);
}

static void build_header(lv_obj_t *parent)
{
    lv_obj_t *header = ui_box(parent);
    lv_obj_set_size(header, UI_SCREEN_W, 44);
    lv_obj_set_style_pad_hor(header, 20, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x1a1e24), 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *left = ui_box(header);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 14, 0);
    ui_label(left, UI_FONT_15, UI_COLOR_TEXT, "Ana");

    /* Page dots: passwords tile (left), Deck (active). */
    lv_obj_t *dots = ui_box(left);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots, 6, 0);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *d = ui_box(dots);
        bool active = (i == 1);
        lv_obj_set_size(d, active ? 18 : 6, 6);
        lv_obj_set_style_radius(d, 3, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(active ? UI_COLOR_ACCENT : 0x343a44), 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    }

    lv_obj_t *right = ui_box(header);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 8, 0);

    w.pc_dot = ui_box(right);
    lv_obj_set_size(w.pc_dot, 8, 8);
    lv_obj_set_style_radius(w.pc_dot, 4, 0);
    lv_obj_set_style_bg_opa(w.pc_dot, LV_OPA_COVER, 0);
    w.pc_label = ui_label(right, UI_FONT_13, UI_COLOR_MUTED, "");
    lv_obj_set_style_pad_right(w.pc_label, 8, 0);
    ui_label(right, UI_FONT_ICON_18, UI_COLOR_MUTED, UI_ICON_WIFI);
    w.clock = ui_label(right, UI_FONT_15, UI_COLOR_TEXT, "--:--");
}

/* ---------------------------------------------------------------- refresh */

static void set_mic_style(bool muted)
{
    w.mic_muted = muted;
    lv_obj_set_style_bg_color(w.mic_key, lv_color_hex(muted ? UI_COLOR_DANGER_BG : UI_COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(w.mic_key, lv_color_hex(muted ? UI_COLOR_DANGER_BORDER : UI_COLOR_BORDER), 0);
    lv_obj_set_style_bg_color(w.mic_chip, lv_color_hex(muted ? 0x4a1c20 : UI_COLOR_SURFACE_2), 0);
    lv_label_set_text(lv_obj_get_child(w.mic_chip, 0), muted ? UI_ICON_MIC_OFF : UI_ICON_MIC);
    lv_obj_set_style_text_color(lv_obj_get_child(w.mic_chip, 0), lv_color_hex(muted ? UI_COLOR_DANGER : UI_COLOR_TEXT_2),
                                0);
    lv_label_set_text(w.mic_label, muted ? "Mikrofon kapalı" : "Mikrofon açık");
    lv_obj_set_style_text_color(w.mic_label, lv_color_hex(muted ? 0xffb3b3 : UI_COLOR_TEXT_2), 0);
}

static void refresh_cb(lv_timer_t *timer)
{
    char buf[48];

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    lv_label_set_text_fmt(w.clock, "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);

    claude_usage_snapshot_t usage;
    claude_usage_get_snapshot(&usage);
    lv_obj_set_style_bg_color(w.pc_dot, lv_color_hex(usage.companion_online ? UI_COLOR_OK : UI_COLOR_FAINT), 0);
    lv_label_set_text(w.pc_label, usage.companion_online ? "PC bağlı" : "PC bağlı değil");
    if (usage.have_data) {
        ui_format_compact(usage.tokens_today, buf, sizeof(buf));
        lv_label_set_text(w.claude_value, buf);
    }

    media_snapshot_t media;
    media_state_get_snapshot(&media);
    if (media.active) {
        lv_label_set_text(w.media_source, media.source);
        lv_label_set_text(w.media_title, media.title[0] ? media.title : "Bilinmeyen parça");
        lv_label_set_text(w.media_artist, media.artist);
        lv_bar_set_value(w.media_progress,
                         media.duration_s > 0 ? (int32_t)(media.position_s * 100 / media.duration_s) : 0, LV_ANIM_OFF);
        lv_label_set_text(w.media_play_icon, media.playing ? UI_ICON_PLAYER_PAUSE : UI_ICON_PLAYER_PLAY);
    } else {
        lv_label_set_text(w.media_source, "");
        lv_label_set_text(w.media_title, "Çalan bir şey yok");
        lv_label_set_text(w.media_artist, "");
        lv_bar_set_value(w.media_progress, 0, LV_ANIM_OFF);
        lv_label_set_text(w.media_play_icon, UI_ICON_PLAYER_PLAY);
    }

    whatsapp_snapshot_t wa;
    whatsapp_state_get_snapshot(&wa);
    if (wa.count > 0) {
        if (wa.count > 9) {
            lv_label_set_text(w.wa_badge_label, "9+");
        } else {
            lv_label_set_text_fmt(w.wa_badge_label, "%d", wa.count);
        }
        lv_obj_clear_flag(w.wa_badge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(w.wa_badge, LV_OBJ_FLAG_HIDDEN);
    }

    mixer_snapshot_t mix;
    mixer_state_get_snapshot(&mix);
    if (mix.mic.present && mix.mic.muted != w.mic_muted) {
        set_mic_style(mix.mic.muted);
    }

    int volume = -1;
    bool muted = false;
    if (mix.output.present) {
        volume = mix.output.volume;
        muted = mix.output.muted;
    } else {
        audio_state_snapshot_t audio;
        audio_state_get_snapshot(&audio);
        if (audio.have_data) {
            volume = audio.volume;
            muted = audio.muted;
        }
    }
    if (volume >= 0) {
        if (muted) {
            lv_label_set_text(w.volume_value, "Sessiz");
        } else {
            lv_label_set_text_fmt(w.volume_value, "%%%d", volume);
        }
        lv_bar_set_value(w.volume_bar, volume, LV_ANIM_OFF);
    }

    prayer_snapshot_t prayer;
    prayer_times_get_snapshot(&prayer);
    if (prayer.have_data && prayer.next_index >= 0) {
        const prayer_time_t *next = &prayer.times[prayer.next_index];
        lv_label_set_text(w.prayer_caption, next->name);
        ui_format_countdown_short(prayer.seconds_to_next, buf, sizeof(buf));
        lv_label_set_text(w.prayer_value, buf);
        lv_label_set_text_fmt(w.prayer_sub, "%s · %02d:%02d", prayer.seconds_to_next >= 3600 ? "saat kaldı" : "dk kaldı",
                              next->hour, next->minute);
    }

    weather_snapshot_t weather;
    weather_state_get_snapshot(&weather);
    if (weather.have_data) {
        lv_label_set_text_fmt(w.weather_value, "%d°", weather.temperature_c);
        if (weather.day_count > 0) {
            lv_label_set_text_fmt(w.weather_sub, "yağmur %%%d", weather.days[0].rain_percent);
        } else {
            lv_label_set_text(w.weather_sub, weather_state_condition_text(weather.weather_code));
        }
    }

    pc_health_snapshot_t health;
    pc_health_state_get_snapshot(&health);
    if (health.have_data) {
        lv_label_set_text_fmt(w.cpu_value, "%%%d", health.cpu_percent);
        lv_bar_set_value(w.cpu_bar, health.cpu_percent, LV_ANIM_OFF);
    }
}

/* ------------------------------------------------------------------ build */

void screen_deck_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    build_header(parent);

    static const lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                       LV_GRID_TEMPLATE_LAST };
    static const lv_coord_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };

    lv_obj_t *grid = ui_box(parent);
    lv_obj_set_width(grid, UI_SCREEN_W);
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_style_pad_top(grid, 14, 0);
    lv_obj_set_style_pad_hor(grid, 20, 0);
    lv_obj_set_style_pad_bottom(grid, 20, 0);
    lv_obj_set_style_pad_row(grid, 12, 0);
    lv_obj_set_style_pad_column(grid, 12, 0);
    lv_obj_set_grid_dsc_array(grid, cols, rows);

    /* Row 1: now playing (2 wide), WhatsApp, YouTube, mic */
    build_media_key(grid);

    lv_obj_t *wa = key_app(grid, 2, 0, UI_TINT_WHATSAPP_BG, UI_TINT_WHATSAPP_FG, UI_ICON_WHATSAPP, "WhatsApp",
                           &A_WHATSAPP);
    w.wa_badge = ui_box(wa);
    lv_obj_add_flag(w.wa_badge, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(w.wa_badge, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_min_width(w.wa_badge, 24, 0);
    lv_obj_set_style_pad_hor(w.wa_badge, 7, 0);
    lv_obj_set_style_radius(w.wa_badge, 12, 0);
    lv_obj_set_style_bg_color(w.wa_badge, lv_color_hex(UI_COLOR_OK), 0);
    lv_obj_set_style_bg_opa(w.wa_badge, LV_OPA_COVER, 0);
    lv_obj_align(w.wa_badge, LV_ALIGN_TOP_RIGHT, 2, -2);
    w.wa_badge_label = ui_label(w.wa_badge, UI_FONT_13, 0x06200f, "");
    lv_obj_center(w.wa_badge_label);

    key_app(grid, 3, 0, UI_TINT_YOUTUBE_BG, UI_TINT_YOUTUBE_FG, UI_ICON_YOUTUBE, "YouTube", &A_YOUTUBE);

    w.mic_key = key_app(grid, 4, 0, UI_COLOR_SURFACE_2, UI_COLOR_TEXT_2, UI_ICON_MIC, "Mikrofon", &A_MIC);
    w.mic_chip = lv_obj_get_child(w.mic_key, 0);
    w.mic_label = lv_obj_get_child(w.mic_key, 1);
    set_mic_style(false);

    /* Row 2: shortcuts */
    key_app(grid, 0, 1, UI_TINT_DISCORD_BG, UI_TINT_DISCORD_FG, UI_ICON_HEADPHONES, "Discord sağır",
            &A_DISCORD_DEAFEN);
    key_app(grid, 1, 1, UI_COLOR_SURFACE_2, UI_COLOR_TEXT_2, UI_ICON_SCREENSHOT, "Ekran görüntüsü", &A_SCREENSHOT);
    key_app(grid, 2, 1, UI_TINT_CODE_BG, UI_TINT_CODE_FG, UI_ICON_CODE, "VS Code", &A_VSCODE);
    key_app(grid, 3, 1, UI_COLOR_SURFACE_2, UI_COLOR_TEXT_2, UI_ICON_TERMINAL, "Terminal", &A_TERMINAL);
    key_app(grid, 4, 1, UI_COLOR_SURFACE_2, UI_COLOR_TEXT_2, UI_ICON_LOCK, "PC'yi kilitle", &A_LOCK);

    /* Row 3: live data */
    lv_obj_t *claude_sub;
    key_live(grid, 0, 2, UI_ICON_SPARKLES, "Claude", &A_CLAUDE, NULL, &w.claude_value, &claude_sub);
    lv_label_set_text(claude_sub, "token bugün");

    key_live(grid, 1, 2, UI_ICON_MOON, "Namaz", &A_PRAYER, &w.prayer_caption, &w.prayer_value, &w.prayer_sub);
    lv_obj_set_style_text_color(w.prayer_value, lv_color_hex(UI_COLOR_ACCENT), 0);

    key_live(grid, 2, 2, UI_ICON_CLOUD, "Hava", &A_WEATHER, NULL, &w.weather_value, &w.weather_sub);

    lv_obj_t *body = key_live(grid, 3, 2, UI_ICON_VOLUME, "Ses", &A_MIXER, NULL, &w.volume_value, NULL);
    w.volume_bar = ui_progress(body, LV_PCT(100), 4, UI_COLOR_ACCENT);

    body = key_live(grid, 4, 2, UI_ICON_GAUGE, "PC", &A_HEALTH, NULL, &w.cpu_value, NULL);
    w.cpu_bar = ui_progress(body, LV_PCT(100), 4, UI_COLOR_TEXT_2);

    lv_timer_create(refresh_cb, 1000, NULL);
    refresh_cb(NULL);
}
