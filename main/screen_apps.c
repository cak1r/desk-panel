#include "screen_apps.h"
#include "ui_theme.h"

typedef struct {
    int id;
    const char *label;
} app_entry_t;

static const app_entry_t APP_ENTRIES[] = {
    { SCREEN_APPS_CLOCK, "Saat" },
    { SCREEN_APPS_PRAYER, "Namaz" },
    { SCREEN_APPS_CLAUDE, "Claude" },
    { SCREEN_APPS_MEDIA, "Medya" },
    { SCREEN_APPS_HEALTH, "PC Sağlık" },
    { SCREEN_APPS_WEATHER, "Hava Durumu" },
};
#define APP_ENTRY_COUNT (sizeof(APP_ENTRIES) / sizeof(APP_ENTRIES[0]))

void screen_apps_create(lv_obj_t *parent, lv_event_cb_t on_app_clicked)
{
    lv_obj_set_style_bg_color(parent, ui_theme_bg_color(), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 24, 0);
    lv_obj_set_style_pad_row(parent, 24, 0);
    lv_obj_set_style_pad_column(parent, 24, 0);

    for (size_t i = 0; i < APP_ENTRY_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        ui_theme_apply_button(btn);
        lv_obj_set_size(btn, 200, 150);
        lv_obj_set_style_radius(btn, 24, 0); /* rounder "tile" look than the generic 12px button radius */
        lv_obj_add_event_cb(btn, on_app_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)APP_ENTRIES[i].id);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, ui_theme_font_emphasis(), 0);
        lv_label_set_text(lbl, APP_ENTRIES[i].label);
        lv_obj_center(lbl);
    }
}
