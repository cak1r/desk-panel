#include "screen_pc_health.h"
#include "pc_health_state.h"
#include "ui_theme.h"

static lv_obj_t *s_cpu_label;
static lv_obj_t *s_ram_label;
static lv_obj_t *s_gpu_label;
static lv_obj_t *s_status_label;

static lv_obj_t *create_stat_card(lv_obj_t *parent, const char *title, lv_obj_t **value_label_out)
{
    lv_obj_t *card = lv_obj_create(parent);
    ui_theme_apply_card(card);
    lv_obj_set_size(card, 200, 160);
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

static void health_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    pc_health_snapshot_t snap;
    pc_health_state_get_snapshot(&snap);

    if (snap.have_data) {
        lv_label_set_text_fmt(s_cpu_label, "%d%%", snap.cpu_percent);
        lv_label_set_text_fmt(s_ram_label, "%d%%", snap.ram_percent);
        if (snap.gpu_percent >= 0) {
            lv_label_set_text_fmt(s_gpu_label, "%d%%", snap.gpu_percent);
        } else {
            lv_label_set_text(s_gpu_label, "--");
        }
    } else {
        lv_label_set_text(s_cpu_label, "--");
        lv_label_set_text(s_ram_label, "--");
        lv_label_set_text(s_gpu_label, "--");
    }

    lv_label_set_text(s_status_label, snap.have_data ? "Companion: online" : "Companion: offline");
    lv_obj_set_style_text_color(s_status_label, snap.have_data ? ui_theme_accent_color() : lv_color_hex(0x8a8f98), 0);
}

void screen_pc_health_create(lv_obj_t *parent)
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
    lv_obj_set_style_pad_column(row, 20, 0);

    create_stat_card(row, "CPU", &s_cpu_label);
    create_stat_card(row, "RAM", &s_ram_label);
    create_stat_card(row, "GPU", &s_gpu_label);

    s_status_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_status_label, ui_theme_font_body(), 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x8a8f98), 0);
    lv_label_set_text(s_status_label, "Companion: offline");

    lv_timer_create(health_timer_cb, 1000, NULL);
    health_timer_cb(NULL);
}
