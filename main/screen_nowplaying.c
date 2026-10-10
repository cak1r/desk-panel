#include "screen_nowplaying.h"
#include "ui.h"
#include "ui_art.h"
#include "ui_format.h"
#include "ui_theme.h"
#include "media_state.h"
#include "mixer_state.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static lv_obj_t *s_title;
static lv_obj_t *s_artist;
static lv_obj_t *s_album;
static lv_obj_t *s_seek;
static lv_obj_t *s_elapsed;
static lv_obj_t *s_total;
static lv_obj_t *s_play_icon;
static lv_obj_t *s_volume_card;
static lv_obj_t *s_volume;
static lv_obj_t *s_volume_label;
static lv_obj_t *s_volume_app;

static float s_position;              /* last shown position, for seek deltas */
static char s_volume_target[40];      /* mixer id of the playing app, "" if unknown */

static void cmd_cb(lv_event_t *e)
{
    media_state_command((const char *)lv_event_get_user_data(e));
}

static void seek_btn_cb(lv_event_t *e)
{
    media_state_seek((int)(intptr_t)lv_event_get_user_data(e));
}

static void seek_released_cb(lv_event_t *e)
{
    media_snapshot_t snap;
    media_state_get_snapshot(&snap);
    if (!snap.can_seek || snap.duration_s <= 0) {
        return;
    }
    float target = lv_slider_get_value(s_seek) * snap.duration_s / 1000.0f;
    media_state_seek((int)(target - s_position));
}

static void volume_changed_cb(lv_event_t *e)
{
    lv_label_set_text_fmt(s_volume_label, "%%%d", (int)lv_slider_get_value(s_volume));
}

static void volume_released_cb(lv_event_t *e)
{
    if (s_volume_target[0]) {
        mixer_state_set_volume(s_volume_target, lv_slider_get_value(s_volume));
    }
}

static lv_obj_t *round_btn(lv_obj_t *parent, int size, uint32_t bg, uint32_t fg, const lv_font_t *font,
                           const char *icon, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, size, size);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(b, bg == UI_COLOR_BG ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(UI_COLOR_SURFACE_3), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_transform_zoom(b, 240, LV_STATE_PRESSED);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *l = ui_label(b, font, fg, icon);
    lv_obj_center(l);
    return b;
}

static void style_slider(lv_obj_t *slider, uint32_t fill, int knob)
{
    lv_obj_remove_style_all(slider);
    lv_obj_set_height(slider, 6);
    lv_obj_set_style_radius(slider, 3, 0);
    lv_obj_set_style_bg_color(slider, lv_color_hex(UI_COLOR_TRACK), 0);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(slider, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(fill), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_hex(UI_COLOR_TEXT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, knob, LV_PART_KNOB);
    lv_obj_set_ext_click_area(slider, 18);
}

static void refresh_cb(lv_timer_t *timer)
{
    media_snapshot_t snap;
    media_state_get_snapshot(&snap);
    char buf[16];

    lv_label_set_text(ui_page_subtitle(UI_PAGE_NOWPLAYING), snap.active ? snap.source : "");
    if (!snap.active) {
        lv_label_set_text(s_title, "Çalan bir şey yok");
        lv_label_set_text(s_artist, "PC'de bir şey çalınca burada görünür");
        lv_label_set_text(s_album, "");
        lv_label_set_text(s_play_icon, UI_ICON_PLAYER_PLAY);
        lv_slider_set_value(s_seek, 0, LV_ANIM_OFF);
        lv_label_set_text(s_elapsed, "");
        lv_label_set_text(s_total, "");
        lv_obj_add_flag(s_volume_card, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(s_title, snap.title[0] ? snap.title : "Bilinmeyen parça");
    lv_label_set_text(s_artist, snap.artist);
    lv_label_set_text(s_album, snap.album);
    lv_label_set_text(s_play_icon, snap.playing ? UI_ICON_PLAYER_PAUSE : UI_ICON_PLAYER_PLAY);

    if (!lv_obj_has_state(s_seek, LV_STATE_PRESSED)) {
        s_position = snap.position_s;
        int32_t permille = snap.duration_s > 0 ? (int32_t)(snap.position_s * 1000 / snap.duration_s) : 0;
        lv_slider_set_value(s_seek, permille, LV_ANIM_OFF);
    }
    ui_format_track_time(snap.position_s, buf, sizeof(buf));
    lv_label_set_text(s_elapsed, buf);
    if (snap.duration_s > 0) {
        ui_format_track_time(snap.duration_s, buf, sizeof(buf));
        lv_label_set_text(s_total, buf);
    } else {
        lv_label_set_text(s_total, "");
    }

    /* The playing app's own mixer channel ("Brave" source -> "brave.exe"). */
    mixer_snapshot_t mix;
    mixer_state_get_snapshot(&mix);
    s_volume_target[0] = '\0';
    for (int i = 0; i < mix.app_count; i++) {
        if (strcasecmp(mix.apps[i].name, snap.source) == 0) {
            strcpy(s_volume_target, mix.apps[i].id);
            lv_label_set_text(s_volume_app, mix.apps[i].name);
            if (!lv_obj_has_state(s_volume, LV_STATE_PRESSED)) {
                lv_slider_set_value(s_volume, mix.apps[i].volume, LV_ANIM_OFF);
                lv_label_set_text_fmt(s_volume_label, "%%%d", mix.apps[i].volume);
            }
            break;
        }
    }
    if (s_volume_target[0]) {
        lv_obj_clear_flag(s_volume_card, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_volume_card, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen_nowplaying_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_left(parent, 32, 0);
    lv_obj_set_style_pad_right(parent, 32, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 0, 0);

    /* Cover + text */
    lv_obj_t *top = ui_box(parent);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(top, 28, 0);
    lv_obj_set_style_pad_top(top, 6, 0);

    ui_art_create(top, 168);

    lv_obj_t *text = ui_box(top);
    lv_obj_set_height(text, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 8, 0);
    s_title = ui_label(text, UI_FONT_32, UI_COLOR_TEXT, "");
    s_artist = ui_label(text, UI_FONT_20_LIGHT, UI_COLOR_TEXT_3, "");
    s_album = ui_label(text, UI_FONT_15, UI_COLOR_MUTED, "");
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);
    lv_label_set_long_mode(s_artist, LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(s_album, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_title, LV_PCT(100));
    lv_obj_set_width(s_artist, LV_PCT(100));
    lv_obj_set_width(s_album, LV_PCT(100));
    lv_obj_set_style_max_height(s_title, 84, 0); /* two lines */

    /* Seekable progress */
    ui_spacer(parent, 1, 26);
    s_seek = lv_slider_create(parent);
    style_slider(s_seek, UI_COLOR_ACCENT, 7);
    lv_obj_set_width(s_seek, LV_PCT(100));
    lv_slider_set_range(s_seek, 0, 1000);
    lv_obj_add_event_cb(s_seek, seek_released_cb, LV_EVENT_RELEASED, NULL);

    ui_spacer(parent, 1, 10);
    lv_obj_t *times = ui_box(parent);
    lv_obj_set_size(times, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(times, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(times, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_elapsed = ui_label(times, UI_FONT_13, UI_COLOR_MUTED, "");
    s_total = ui_label(times, UI_FONT_13, UI_COLOR_MUTED, "");

    /* Transport + app volume */
    lv_obj_t *bottom = ui_box(parent);
    lv_obj_set_width(bottom, LV_PCT(100));
    lv_obj_set_flex_grow(bottom, 1);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bottom, 24, 0);

    lv_obj_t *transport = ui_box(bottom);
    lv_obj_set_size(transport, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(transport, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(transport, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(transport, 14, 0);
    round_btn(transport, 56, UI_COLOR_BG, UI_COLOR_TEXT_3, UI_FONT_ICON_24, UI_ICON_SEEK_BACK_10, seek_btn_cb,
              (void *)(intptr_t)-10);
    round_btn(transport, 64, UI_COLOR_SURFACE, UI_COLOR_TEXT, UI_FONT_ICON_24, UI_ICON_PLAYER_TRACK_PREV, cmd_cb,
              "previous");
    lv_obj_t *play = round_btn(transport, 84, UI_COLOR_ACCENT, UI_COLOR_ON_ACCENT, UI_FONT_ICON_36,
                               UI_ICON_PLAYER_PLAY, cmd_cb, "play_pause");
    s_play_icon = lv_obj_get_child(play, 0);
    round_btn(transport, 64, UI_COLOR_SURFACE, UI_COLOR_TEXT, UI_FONT_ICON_24, UI_ICON_PLAYER_TRACK_NEXT, cmd_cb,
              "next");
    round_btn(transport, 56, UI_COLOR_BG, UI_COLOR_TEXT_3, UI_FONT_ICON_24, UI_ICON_SEEK_FORWARD_10, seek_btn_cb,
              (void *)(intptr_t)10);

    s_volume_card = ui_box(bottom);
    ui_theme_apply_surface(s_volume_card);
    lv_obj_set_style_radius(s_volume_card, 16, 0);
    lv_obj_set_style_pad_hor(s_volume_card, 16, 0);
    lv_obj_set_style_pad_ver(s_volume_card, 14, 0);
    lv_obj_set_height(s_volume_card, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(s_volume_card, 1);
    lv_obj_set_flex_flow(s_volume_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_volume_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_volume_card, 12, 0);
    ui_label(s_volume_card, UI_FONT_ICON_18, UI_COLOR_MUTED, UI_ICON_VOLUME);
    s_volume = lv_slider_create(s_volume_card);
    style_slider(s_volume, UI_COLOR_TEXT_2, 6);
    lv_obj_set_flex_grow(s_volume, 1);
    lv_slider_set_range(s_volume, 0, 100);
    lv_obj_add_event_cb(s_volume, volume_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_volume, volume_released_cb, LV_EVENT_RELEASED, NULL);
    s_volume_label = ui_label(s_volume_card, UI_FONT_15, UI_COLOR_TEXT, "");
    s_volume_app = ui_label(s_volume_card, UI_FONT_13, UI_COLOR_MUTED, "");
    lv_obj_add_flag(s_volume_card, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(refresh_cb, 500, NULL);
    refresh_cb(NULL);
}
