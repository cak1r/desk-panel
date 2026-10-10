#include "screen_claude.h"
#include "claude_usage.h"
#include "ui_theme.h"

static lv_obj_t *s_requests_label;
static lv_obj_t *s_tokens_label;
static lv_obj_t *s_status_label;

static lv_obj_t *create_stat_card(lv_obj_t *parent, const char *title, lv_obj_t **value_label_out)
{
    lv_obj_t *card = lv_obj_create(parent);
    ui_theme_apply_card(card);
    lv_obj_set_size(card, 320, 160);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 8, 0);

    lv_obj_t *title_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(title_lbl, ui_theme_font_body(), 0);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x8a8f98), 0);

    lv_obj_t *value_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(value_lbl, ui_theme_font_emphasis(), 0);
    lv_label_set_text(value_lbl, "--");
    lv_obj_set_style_text_color(value_lbl, ui_theme_accent_color(), 0);
    *value_label_out = value_lbl;

    return card;
}

static void claude_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    claude_usage_snapshot_t snap;
    claude_usage_get_snapshot(&snap);

    if (snap.have_data) {
        lv_label_set_text_fmt(s_requests_label, "%" LV_PRId32, snap.requests_today);
        lv_label_set_text_fmt(s_tokens_label, "%" LV_PRId32, snap.tokens_today);
    } else {
        lv_label_set_text(s_requests_label, "--");
        lv_label_set_text(s_tokens_label, "--");
    }

    lv_label_set_text(s_status_label, snap.companion_online ? "Companion: online" : "Companion: offline");
    lv_obj_set_style_text_color(s_status_label, snap.companion_online ? ui_theme_accent_color() : lv_color_hex(0x8a8f98), 0);
}

void screen_claude_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, ui_theme_bg_color(), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 24, 0);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 24, 0);

    create_stat_card(row, "Bugunku istek", &s_requests_label);
    create_stat_card(row, "Bugunku token", &s_tokens_label);

    s_status_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_status_label, ui_theme_font_body(), 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x8a8f98), 0);
    lv_label_set_text(s_status_label, "Companion: offline");

    lv_timer_create(claude_timer_cb, 1000, NULL);
    claude_timer_cb(NULL);
}
