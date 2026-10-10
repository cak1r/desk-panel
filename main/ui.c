#include "ui.h"
#include "ui_art.h"
#include "ui_theme.h"
#include "screen_clock.h"
#include "screen_deck.h"
#include "screen_nowplaying.h"
#include "screen_mixer.h"
#include "screen_youtube.h"
#include "screen_whatsapp.h"
#include "screen_prayer.h"
#include "screen_claude.h"
#include "screen_pc_health.h"
#include "screen_weather.h"
#include "screen_passwords.h"
#include "prayer_times.h"
#include "claude_usage.h"
#include "audio_state.h"
#include "pc_health_state.h"
#include "weather_state.h"
#include "media_state.h"
#include "mixer_state.h"
#include "profile_state.h"
#include "whatsapp_state.h"
#include "hadith_state.h"

#include "esp_log.h"

#include <string.h>

/* Idle behaviour thresholds */
#define IDLE_RETURN_MS (5 * 60 * 1000) /* inactivity before the clock curtain slides down */
/* An auto-profile switch waits until the panel hasn't been touched for this
 * long, so it never yanks a screen away mid-interaction. */
#define PROFILE_SWITCH_IDLE_MS 2000

static const char *UI_TAG = "ui";

typedef struct {
    const char *title;
    const char *subtitle;
    void (*create)(lv_obj_t *content);
} page_def_t;

static const page_def_t PAGES[UI_PAGE_COUNT] = {
    [UI_PAGE_NOWPLAYING] = { "Şimdi çalan", "", screen_nowplaying_create },
    [UI_PAGE_MIXER] = { "Mikser", "", screen_mixer_create },
    [UI_PAGE_YOUTUBE] = { "YouTube", "", screen_youtube_create },
    [UI_PAGE_WHATSAPP] = { "WhatsApp", "", screen_whatsapp_create },
    [UI_PAGE_PRAYER] = { "Namaz Vakitleri", "İstanbul · Diyanet", screen_prayer_create },
    [UI_PAGE_WEATHER] = { "Hava Durumu", "İstanbul · Open-Meteo", screen_weather_create },
    [UI_PAGE_CLAUDE] = { "Claude Kullanımı", "", screen_claude_create },
    [UI_PAGE_HEALTH] = { "PC Sağlığı", "", screen_pc_health_create },
};

static lv_obj_t *s_tileview;
static lv_obj_t *s_tile_deck;
static lv_obj_t *s_tile_passwords;
static lv_obj_t *s_clock_curtain;
static lv_obj_t *s_pages[UI_PAGE_COUNT];
static lv_obj_t *s_subtitles[UI_PAGE_COUNT];
static int s_active_page = -1;
static bool s_active_page_is_auto; /* opened by deskpanel/profile, not by a tap */
static bool s_clock_shown = true;

static uint32_t s_profile_version;
static bool s_profile_pending;

/* ------------------------------------------------------------------ pages */

void ui_close_page(void)
{
    if (s_active_page >= 0) {
        lv_obj_add_flag(s_pages[s_active_page], LV_OBJ_FLAG_HIDDEN);
        s_active_page = -1;
        s_active_page_is_auto = false;
    }
}

static void open_page(ui_page_t page, bool is_auto)
{
    if (page >= UI_PAGE_COUNT) {
        return;
    }
    if (s_active_page != (int)page) {
        ui_close_page();
        s_active_page = page;
        lv_obj_clear_flag(s_pages[page], LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_pages[page]);
        if (s_clock_shown) {
            lv_obj_move_foreground(s_clock_curtain);
        }
    }
    s_active_page_is_auto = is_auto;
}

void ui_open_page(ui_page_t page)
{
    open_page(page, false);
}

lv_obj_t *ui_page_subtitle(ui_page_t page)
{
    return page < UI_PAGE_COUNT ? s_subtitles[page] : NULL;
}

static void home_btn_cb(lv_event_t *e)
{
    ui_close_page();
}

static lv_obj_t *create_page(lv_obj_t *scr, ui_page_t id)
{
    const page_def_t *def = &PAGES[id];

    lv_obj_t *page = ui_box(scr);
    lv_obj_set_size(page, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_add_flag(page, LV_OBJ_FLAG_CLICKABLE); /* swallow touches meant for what's underneath */
    lv_obj_clear_flag(page, LV_OBJ_FLAG_EVENT_BUBBLE);
    ui_theme_apply_bg(page);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *header = ui_box(page);
    lv_obj_set_size(header, UI_SCREEN_W, UI_HEADER_H);
    lv_obj_set_style_pad_hor(header, 20, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 12, 0);

    ui_label(header, UI_FONT_18, UI_COLOR_TEXT, def->title);
    lv_obj_t *subtitle = ui_label(header, UI_FONT_13, UI_COLOR_MUTED, def->subtitle);
    lv_obj_set_flex_grow(subtitle, 1);

    lv_obj_t *home = lv_btn_create(header);
    ui_theme_apply_key(home);
    lv_obj_set_style_radius(home, 10, 0);
    lv_obj_set_style_pad_hor(home, 12, 0);
    lv_obj_set_style_pad_ver(home, 0, 0);
    lv_obj_set_height(home, 36);
    lv_obj_set_ext_click_area(home, 8);
    lv_obj_set_flex_flow(home, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(home, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(home, 6, 0);
    lv_obj_add_event_cb(home, home_btn_cb, LV_EVENT_CLICKED, NULL);
    ui_label(home, UI_FONT_ICON_18, UI_COLOR_TEXT_2, UI_ICON_GRID);
    ui_label(home, UI_FONT_13, UI_COLOR_TEXT_2, "Ana");

    lv_obj_t *content = ui_box(page);
    lv_obj_set_width(content, UI_SCREEN_W);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_pad_left(content, 20, 0);
    lv_obj_set_style_pad_right(content, 20, 0);
    lv_obj_set_style_pad_top(content, 2, 0);
    lv_obj_set_style_pad_bottom(content, 20, 0);

    s_subtitles[id] = subtitle;
    def->create(content);
    return page;
}

/* --------------------------------------------------------- clock curtain */

static void tileview_changed_cb(lv_event_t *e)
{
    if (lv_tileview_get_tile_act(s_tileview) != s_tile_passwords) {
        screen_passwords_lock();
    }
}

static void curtain_set_y_anim_cb(void *var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)v);
}

static void animate_curtain(int32_t from, int32_t to)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_clock_curtain);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, curtain_set_y_anim_cb);
    lv_anim_start(&a);
}

static void hide_curtain(void)
{
    if (!s_clock_shown) {
        return;
    }
    s_clock_shown = false;
    animate_curtain(0, -UI_SCREEN_H);
}

static void show_curtain(void)
{
    if (s_clock_shown) {
        return;
    }
    s_clock_shown = true;

    /* Whatever is revealed once the curtain is dismissed again should be
     * the Deck, not whatever page/tile was left open. */
    ui_close_page();
    lv_obj_set_tile_id(s_tileview, 1, 0, LV_ANIM_OFF);
    lv_obj_move_foreground(s_clock_curtain);
    animate_curtain(-UI_SCREEN_H, 0);
}

static void curtain_event_cb(lv_event_t *e)
{
    hide_curtain();
}

/* ------------------------------------------------------- auto profiles */

static int page_for_profile(const char *profile)
{
    if (strcmp(profile, "youtube") == 0) {
        return UI_PAGE_YOUTUBE;
    }
    if (strcmp(profile, "ytmusic") == 0) {
        return UI_PAGE_NOWPLAYING;
    }
    if (strcmp(profile, "whatsapp") == 0) {
        return UI_PAGE_WHATSAPP;
    }
    return -1;
}

static void apply_profile(void)
{
    char profile[24];
    profile_state_get(profile, sizeof(profile));
    int page = page_for_profile(profile);

    if (page >= 0) {
        /* Don't override a page the user opened by hand. */
        if (s_active_page < 0 || s_active_page_is_auto) {
            open_page((ui_page_t)page, true);
        }
    } else if (s_active_page_is_auto) {
        /* The app that triggered the page lost focus: go back to the Deck. */
        ui_close_page();
    }
}

/* ----------------------------------------------------------------- timers */

static void ui_tick_cb(lv_timer_t *timer)
{
    uint32_t inactive_ms = lv_disp_get_inactive_time(NULL);

    if (!s_clock_shown && inactive_ms >= IDLE_RETURN_MS) {
        show_curtain();
    }

    char profile[24];
    uint32_t v = profile_state_get(profile, sizeof(profile));
    if (v != s_profile_version) {
        s_profile_version = v;
        s_profile_pending = true;
    }
    /* The clock curtain means nobody's looking at the panel: don't wake it
     * up for a focus change, just remember the latest profile. */
    if (s_profile_pending && !s_clock_shown && inactive_ms >= PROFILE_SWITCH_IDLE_MS) {
        s_profile_pending = false;
        apply_profile();
    }

    ui_art_poll();
}

/* ------------------------------------------------------------------- init */

void ui_init(void)
{
    ui_theme_init();
    prayer_times_init();
    claude_usage_init();
    audio_state_init();
    pc_health_state_init();
    weather_state_init();
    media_state_init();
    mixer_state_init();
    profile_state_init();
    whatsapp_state_init();
    hadith_state_init();

    lv_obj_t *scr = lv_scr_act();
    ui_theme_apply_bg(scr);

    s_tileview = lv_tileview_create(scr);
    lv_obj_set_size(s_tileview, UI_SCREEN_W, UI_SCREEN_H);
    ui_theme_apply_bg(s_tileview);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);

    s_tile_passwords = lv_tileview_add_tile(s_tileview, 0, 0, LV_DIR_HOR);
    s_tile_deck = lv_tileview_add_tile(s_tileview, 1, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_opa(s_tile_passwords, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(s_tile_deck, LV_OPA_TRANSP, 0);

    screen_deck_create(s_tile_deck);
    screen_passwords_create(s_tile_passwords);

    lv_obj_add_event_cb(s_tileview, tileview_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_tile_id(s_tileview, 1, 0, LV_ANIM_OFF); /* start on the Deck */

    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        s_pages[i] = create_page(scr, (ui_page_t)i);
    }

    /* Clock curtain: idle "lock screen" -- starts down over everything,
     * slides up out of the way on tap, and back down after IDLE_RETURN_MS. */
    s_clock_curtain = ui_box(scr);
    lv_obj_set_size(s_clock_curtain, UI_SCREEN_W, UI_SCREEN_H);
    ui_theme_apply_bg(s_clock_curtain);
    lv_obj_add_flag(s_clock_curtain, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_clock_curtain, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(s_clock_curtain, curtain_event_cb, LV_EVENT_PRESSED, NULL);
    screen_clock_create(s_clock_curtain);

    s_profile_version = profile_state_get((char[24]){ 0 }, 24);
    lv_timer_create(ui_tick_cb, 250, NULL);

    ESP_LOGI(UI_TAG, "UI initialized");
}
