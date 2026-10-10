#include "screen_media.h"
#include "audio_state.h"
#include "ui_theme.h"
#include "usb_hid.h"

#include "class/hid/hid.h"

#include <stdlib.h>

/* USB HID consumer control only has discrete volume up/down steps, not an
 * absolute "set to X%" command -- assume the host's default step size to
 * approximate a drag-to-target. The live MQTT readout corrects the slider
 * to the real resulting value shortly after. */
#define VOLUME_STEP_PERCENT 2

static lv_obj_t *s_slider;
static lv_obj_t *s_percent_label;
static int s_last_known_volume = 60;

static void prev_cb(lv_event_t *e)
{
    (void)e;
    usb_hid_send_consumer(HID_USAGE_CONSUMER_SCAN_PREVIOUS_TRACK);
}

static void play_pause_cb(lv_event_t *e)
{
    (void)e;
    usb_hid_send_consumer(HID_USAGE_CONSUMER_PLAY_PAUSE);
}

static void next_cb(lv_event_t *e)
{
    (void)e;
    usb_hid_send_consumer(HID_USAGE_CONSUMER_SCAN_NEXT_TRACK);
}

static void slider_value_changed_cb(lv_event_t *e)
{
    (void)e;
    lv_label_set_text_fmt(s_percent_label, "%d%%", (int)lv_slider_get_value(s_slider));
}

static void slider_released_cb(lv_event_t *e)
{
    (void)e;
    int target = lv_slider_get_value(s_slider);
    int delta = target - s_last_known_volume;
    int presses = (abs(delta) + VOLUME_STEP_PERCENT / 2) / VOLUME_STEP_PERCENT;

    uint16_t usage = (delta > 0) ? HID_USAGE_CONSUMER_VOLUME_INCREMENT : HID_USAGE_CONSUMER_VOLUME_DECREMENT;
    for (int i = 0; i < presses; i++) {
        usb_hid_send_consumer(usage);
    }

    s_last_known_volume = target;
}

static void audio_sync_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    audio_state_snapshot_t snap;
    audio_state_get_snapshot(&snap);

    if (!snap.have_data || lv_obj_has_state(s_slider, LV_STATE_PRESSED)) {
        return;
    }

    lv_slider_set_value(s_slider, snap.volume, LV_ANIM_ON);
    s_last_known_volume = snap.volume;

    if (snap.muted) {
        lv_label_set_text(s_percent_label, "Sessiz");
    } else {
        lv_label_set_text_fmt(s_percent_label, "%d%%", snap.volume);
    }
}

static lv_obj_t *create_icon_btn(lv_obj_t *parent, const char *icon, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    ui_theme_apply_button(btn);
    lv_obj_set_size(btn, 72, 72);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, ui_theme_font_icons(), 0);
    lv_label_set_text(label, icon);
    lv_obj_center(label);

    return btn;
}

void screen_media_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, ui_theme_bg_color(), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 16, 0);

    s_percent_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_percent_label, ui_theme_font_emphasis(), 0);
    lv_obj_set_style_text_color(s_percent_label, ui_theme_accent_color(), 0);
    lv_label_set_text_fmt(s_percent_label, "%d%%", s_last_known_volume);

    s_slider = lv_slider_create(parent);
    lv_obj_set_width(s_slider, 400);
    lv_slider_set_range(s_slider, 0, 100);
    lv_slider_set_value(s_slider, s_last_known_volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider, ui_theme_accent_color(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, ui_theme_accent_color(), LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, slider_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_slider, slider_released_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 24, 0);

    create_icon_btn(row, UI_ICON_PLAYER_TRACK_PREV, prev_cb);
    create_icon_btn(row, UI_ICON_PLAYER_PLAY, play_pause_cb);
    create_icon_btn(row, UI_ICON_PLAYER_TRACK_NEXT, next_cb);

    lv_timer_create(audio_sync_timer_cb, 1000, NULL);
}
