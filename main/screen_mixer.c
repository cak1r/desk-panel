#include "screen_mixer.h"
#include "ui.h"
#include "ui_theme.h"
#include "mixer_state.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

/* Output + up to MIXER_MAX_APPS - 2 apps + mic fit across 760 px. */
#define MAX_CHANNELS 8

typedef struct {
    lv_obj_t *card;
    lv_obj_t *chip;
    lv_obj_t *chip_label;
    lv_obj_t *name;
    lv_obj_t *value;
    lv_obj_t *slider;
    lv_obj_t *mute_btn;
    lv_obj_t *mute_icon;
    char target[40];
    bool muted;
} channel_t;

static channel_t s_ch[MAX_CHANNELS];
static lv_obj_t *s_empty;

static void slider_changed_cb(lv_event_t *e)
{
    channel_t *ch = lv_event_get_user_data(e);
    lv_label_set_text_fmt(ch->value, "%%%d", (int)lv_slider_get_value(ch->slider));
}

static void slider_released_cb(lv_event_t *e)
{
    channel_t *ch = lv_event_get_user_data(e);
    mixer_state_set_volume(ch->target, lv_slider_get_value(ch->slider));
}

static void mute_cb(lv_event_t *e)
{
    channel_t *ch = lv_event_get_user_data(e);
    mixer_state_set_mute(ch->target, !ch->muted);
}

static void tint_for(const char *name, uint32_t *bg, uint32_t *fg)
{
    struct {
        const char *name;
        uint32_t bg, fg;
    } static const TINTS[] = {
        { "Brave", UI_TINT_BRAVE_BG, UI_TINT_BRAVE_FG },
        { "Chrome", UI_TINT_CODE_BG, UI_TINT_CODE_FG },
        { "Discord", UI_TINT_DISCORD_BG, UI_TINT_DISCORD_FG },
        { "WhatsApp", UI_TINT_WHATSAPP_BG, UI_TINT_WHATSAPP_FG },
        { "Spotify", UI_TINT_WHATSAPP_BG, UI_TINT_WHATSAPP_FG },
    };
    for (size_t i = 0; i < sizeof(TINTS) / sizeof(TINTS[0]); i++) {
        if (strcasecmp(name, TINTS[i].name) == 0) {
            *bg = TINTS[i].bg;
            *fg = TINTS[i].fg;
            return;
        }
    }
    *bg = UI_COLOR_SURFACE_2;
    *fg = UI_COLOR_TEXT_2;
}

static void set_channel(channel_t *ch, const char *target, const char *name, const char *chip_text,
                        bool chip_is_icon, int volume, bool muted)
{
    lv_obj_clear_flag(ch->card, LV_OBJ_FLAG_HIDDEN);
    strncpy(ch->target, target, sizeof(ch->target) - 1);
    lv_label_set_text(ch->name, name);

    uint32_t bg, fg;
    tint_for(name, &bg, &fg);
    lv_obj_set_style_bg_color(ch->chip, lv_color_hex(bg), 0);
    lv_obj_set_style_text_color(ch->chip_label, lv_color_hex(fg), 0);
    lv_obj_set_style_text_font(ch->chip_label, chip_is_icon ? UI_FONT_ICON_24 : UI_FONT_18, 0);
    lv_label_set_text(ch->chip_label, chip_text);

    ch->muted = muted;
    if (!lv_obj_has_state(ch->slider, LV_STATE_PRESSED)) {
        lv_slider_set_value(ch->slider, volume, LV_ANIM_OFF);
        if (muted) {
            lv_label_set_text(ch->value, "sessiz");
        } else {
            lv_label_set_text_fmt(ch->value, "%%%d", volume);
        }
    }
    lv_obj_set_style_bg_color(ch->slider, lv_color_hex(muted ? 0x3a3f48 : UI_COLOR_ACCENT), LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(ch->mute_btn, lv_color_hex(muted ? UI_COLOR_DANGER_BG : UI_COLOR_SURFACE_2), 0);
    lv_obj_set_style_border_color(ch->mute_btn, lv_color_hex(muted ? UI_COLOR_DANGER_BORDER : UI_COLOR_BORDER), 0);
    lv_obj_set_style_text_color(ch->mute_icon, lv_color_hex(muted ? UI_COLOR_DANGER : UI_COLOR_MUTED), 0);
}

static void refresh_cb(lv_timer_t *timer)
{
    mixer_snapshot_t snap;
    mixer_state_get_snapshot(&snap);

    lv_obj_t *subtitle = ui_page_subtitle(UI_PAGE_MIXER);
    if (snap.output.present) {
        lv_label_set_text_fmt(subtitle, "Çıkış: %s", snap.output.name);
    } else {
        lv_label_set_text(subtitle, "");
    }

    int n = 0;
    if (snap.output.present) {
        set_channel(&s_ch[n++], MIXER_TARGET_OUTPUT, "Sistem", UI_ICON_VOLUME, true, snap.output.volume,
                    snap.output.muted);
    }
    for (int i = 0; i < snap.app_count && n < MAX_CHANNELS - 1; i++) {
        const mixer_app_t *a = &snap.apps[i];
        char initial[5] = { 0 };
        /* First UTF-8 character of the name. */
        int len = ((unsigned char)a->name[0] & 0x80) ? (((unsigned char)a->name[0] & 0xE0) == 0xC0 ? 2 : 3) : 1;
        memcpy(initial, a->name, len);
        set_channel(&s_ch[n++], a->id, a->name, initial, false, a->volume, a->muted);
    }
    if (snap.mic.present) {
        set_channel(&s_ch[n++], MIXER_TARGET_MIC, "Mikrofon", UI_ICON_MIC, true, snap.mic.volume, snap.mic.muted);
    }
    for (int i = n; i < MAX_CHANNELS; i++) {
        lv_obj_add_flag(s_ch[i].card, LV_OBJ_FLAG_HIDDEN);
    }

    if (n == 0) {
        lv_obj_clear_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    }
}

static void build_channel(lv_obj_t *parent, channel_t *ch)
{
    ch->card = ui_box(parent);
    ui_theme_apply_surface(ch->card);
    lv_obj_set_height(ch->card, LV_PCT(100));
    lv_obj_set_flex_grow(ch->card, 1);
    lv_obj_set_style_pad_hor(ch->card, 8, 0);
    lv_obj_set_flex_flow(ch->card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ch->card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ch->card, 10, 0);

    ch->chip = ui_icon_chip(ch->card, 40, UI_COLOR_SURFACE_2, UI_COLOR_TEXT_2, UI_FONT_18, "");
    ch->chip_label = lv_obj_get_child(ch->chip, 0);

    lv_obj_t *labels = ui_box(ch->card);
    lv_obj_set_size(labels, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(labels, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(labels, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ch->name = ui_label(labels, UI_FONT_13, UI_COLOR_TEXT, "");
    lv_label_set_long_mode(ch->name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ch->name, LV_PCT(100));
    lv_obj_set_style_text_align(ch->name, LV_TEXT_ALIGN_CENTER, 0);
    ch->value = ui_label(labels, UI_FONT_13, UI_COLOR_MUTED, "");

    ch->slider = lv_slider_create(ch->card);
    lv_obj_remove_style_all(ch->slider);
    lv_obj_set_width(ch->slider, 56);
    lv_obj_set_flex_grow(ch->slider, 1);
    lv_slider_set_range(ch->slider, 0, 100);
    lv_obj_set_style_radius(ch->slider, 16, 0);
    lv_obj_set_style_bg_color(ch->slider, lv_color_hex(UI_COLOR_SURFACE_3), 0);
    lv_obj_set_style_bg_opa(ch->slider, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ch->slider, 16, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ch->slider, lv_color_hex(UI_COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ch->slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_add_event_cb(ch->slider, slider_changed_cb, LV_EVENT_VALUE_CHANGED, ch);
    lv_obj_add_event_cb(ch->slider, slider_released_cb, LV_EVENT_RELEASED, ch);

    ch->mute_btn = lv_obj_create(ch->card);
    ui_theme_apply_key(ch->mute_btn);
    lv_obj_set_size(ch->mute_btn, 56, 44);
    lv_obj_set_style_radius(ch->mute_btn, 12, 0);
    lv_obj_set_style_pad_all(ch->mute_btn, 0, 0);
    lv_obj_add_event_cb(ch->mute_btn, mute_cb, LV_EVENT_CLICKED, ch);
    ch->mute_icon = ui_label(ch->mute_btn, UI_FONT_ICON_24, UI_COLOR_MUTED, UI_ICON_VOLUME_OFF);
    lv_obj_center(ch->mute_icon);

    lv_obj_add_flag(ch->card, LV_OBJ_FLAG_HIDDEN);
}

void screen_mixer_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(parent, 10, 0);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        build_channel(parent, &s_ch[i]);
    }

    s_empty = ui_label(parent, UI_FONT_15, UI_COLOR_MUTED, "Companion'dan ses bilgisi bekleniyor…");
    lv_obj_add_flag(s_empty, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_center(s_empty);

    lv_timer_create(refresh_cb, 500, NULL);
    refresh_cb(NULL);
}
