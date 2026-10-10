#include "ui.h"
#include "ui_theme.h"
#include "screen_clock.h"
#include "screen_apps.h"
#include "screen_prayer.h"
#include "screen_claude.h"
#include "screen_media.h"
#include "screen_pc_health.h"
#include "screen_weather.h"
#include "screen_passwords.h"
#include "prayer_times.h"
#include "claude_usage.h"
#include "audio_state.h"
#include "pc_health_state.h"
#include "weather_state.h"

#include "esp_log.h"

#define SCREEN_W 800
#define SCREEN_H 480

/* Idle behaviour thresholds */
#define IDLE_RETURN_MS (5 * 60 * 1000) /* inactivity before the clock curtain slides down */

#define DETAIL_COUNT 5

static const char *UI_TAG = "ui";

static lv_obj_t *s_tileview;
static lv_obj_t *s_tile_apps;
static lv_obj_t *s_tile_passwords;
static lv_obj_t *s_clock_curtain;
static lv_obj_t *s_detail_overlay[DETAIL_COUNT];
static int s_active_detail = -1;
static bool s_clock_shown = true;

static void close_active_detail(void)
{
    if (s_active_detail >= 0) {
        lv_obj_add_flag(s_detail_overlay[s_active_detail], LV_OBJ_FLAG_HIDDEN);
        s_active_detail = -1;
    }
}

static void show_curtain(void);

static void app_btn_clicked_cb(lv_event_t *e)
{
    int id = (int)(intptr_t)lv_event_get_user_data(e);

    if (id == SCREEN_APPS_CLOCK) {
        /* Reuse the existing clock curtain rather than a second
         * screen_clock_create() instance -- that module keeps its labels
         * in file-scope statics, so a second instance would steal updates
         * away from the first. */
        show_curtain();
        return;
    }

    if (id < 0 || id >= DETAIL_COUNT) {
        return;
    }
    close_active_detail();
    s_active_detail = id;
    lv_obj_clear_flag(s_detail_overlay[id], LV_OBJ_FLAG_HIDDEN);
}

static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    close_active_detail();
}

static void tileview_changed_cb(lv_event_t *e)
{
    (void)e;
    if (lv_tileview_get_tile_act(s_tileview) != s_tile_passwords) {
        screen_passwords_lock();
    }
}

static void curtain_set_y_anim_cb(void *var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)v);
}

static void hide_curtain(void)
{
    if (!s_clock_shown) {
        return;
    }
    s_clock_shown = false;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_clock_curtain);
    lv_anim_set_values(&a, 0, -SCREEN_H);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, curtain_set_y_anim_cb);
    lv_anim_start(&a);
}

static void show_curtain(void)
{
    if (s_clock_shown) {
        return;
    }
    s_clock_shown = true;

    /* Whatever is revealed once the curtain is dismissed again should be
     * the home grid, not whatever detail screen/tile was left open. */
    close_active_detail();
    lv_obj_set_tile_id(s_tileview, 1, 0, LV_ANIM_OFF);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_clock_curtain);
    lv_anim_set_values(&a, -SCREEN_H, 0);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, curtain_set_y_anim_cb);
    lv_anim_start(&a);
}

static void curtain_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) {
        return;
    }
    hide_curtain();
}

static void idle_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    uint32_t inactive_ms = lv_disp_get_inactive_time(NULL);

    if (!s_clock_shown && inactive_ms >= IDLE_RETURN_MS) {
        show_curtain();
    }
}

/* Full-tile overlay: a back button + title bar above a content area that
 * the caller fills with an existing screen_*_create(). Hidden until its
 * grid button is tapped. */
static lv_obj_t *create_detail_overlay(lv_obj_t *scr, const char *title, lv_obj_t **content_out)
{
    lv_obj_t *overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(overlay, 0, 0);
    ui_theme_apply_bg_gradient(overlay);
    lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *bar = lv_obj_create(overlay);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, SCREEN_W, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(bar, 16, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 16, 0);

    lv_obj_t *back_btn = lv_btn_create(bar);
    ui_theme_apply_button(back_btn);
    lv_obj_set_size(back_btn, 90, 48);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_obj_set_style_text_font(back_lbl, ui_theme_font_body(), 0);
    lv_label_set_text(back_lbl, "< Geri");
    lv_obj_center(back_lbl);

    lv_obj_t *title_lbl = lv_label_create(bar);
    lv_obj_set_style_text_font(title_lbl, ui_theme_font_body(), 0);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x8a8f98), 0);
    lv_label_set_text(title_lbl, title);

    lv_obj_t *content = lv_obj_create(overlay);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, SCREEN_W, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(content, 1);

    *content_out = content;
    return overlay;
}

void ui_init(void)
{
    ui_theme_init();
    prayer_times_init();
    claude_usage_init();
    audio_state_init();
    pc_health_state_init();
    weather_state_init();

    lv_obj_t *scr = lv_scr_act();
    ui_theme_apply_bg_gradient(scr);

    s_tileview = lv_tileview_create(scr);
    lv_obj_set_size(s_tileview, SCREEN_W, SCREEN_H);
    ui_theme_apply_bg_gradient(s_tileview);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);

    s_tile_passwords = lv_tileview_add_tile(s_tileview, 0, 0, LV_DIR_HOR);
    s_tile_apps = lv_tileview_add_tile(s_tileview, 1, 0, LV_DIR_HOR);
    /* Tiles paint their own flat bg_color underneath -- make them
     * transparent so the tileview's gradient behind them shows through. */
    lv_obj_set_style_bg_opa(s_tile_passwords, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(s_tile_apps, LV_OPA_TRANSP, 0);

    screen_apps_create(s_tile_apps, app_btn_clicked_cb);
    screen_passwords_create(s_tile_passwords);

    lv_obj_add_event_cb(s_tileview, tileview_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_tile_id(s_tileview, 1, 0, LV_ANIM_OFF); /* start on the apps grid */

    lv_obj_t *content;

    s_detail_overlay[SCREEN_APPS_PRAYER] = create_detail_overlay(scr, "Namaz Vakitleri", &content);
    screen_prayer_create(content);

    s_detail_overlay[SCREEN_APPS_CLAUDE] = create_detail_overlay(scr, "Claude Kullanimi", &content);
    screen_claude_create(content);

    s_detail_overlay[SCREEN_APPS_MEDIA] = create_detail_overlay(scr, "Medya", &content);
    screen_media_create(content);

    s_detail_overlay[SCREEN_APPS_HEALTH] = create_detail_overlay(scr, "PC Sagligi", &content);
    screen_pc_health_create(content);

    s_detail_overlay[SCREEN_APPS_WEATHER] = create_detail_overlay(scr, "Hava Durumu", &content);
    screen_weather_create(content);

    /* Clock curtain: idle "lock screen" -- starts down over the grid, slides
     * up out of the way on tap, and slides back down after IDLE_RETURN_MS. */
    s_clock_curtain = lv_obj_create(scr);
    lv_obj_remove_style_all(s_clock_curtain);
    lv_obj_set_size(s_clock_curtain, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_clock_curtain, 0, 0);
    ui_theme_apply_bg_gradient(s_clock_curtain);
    lv_obj_add_flag(s_clock_curtain, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_clock_curtain, curtain_event_cb, LV_EVENT_PRESSED, NULL);
    screen_clock_create(s_clock_curtain);

    lv_timer_create(idle_timer_cb, 1000, NULL);

    ESP_LOGI(UI_TAG, "UI initialized");
}
