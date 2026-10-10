#include "ui_art.h"
#include "media_state.h"
#include "ui_theme.h"

#include <stdlib.h>

#define MAX_WIDGETS 4

typedef struct {
    lv_obj_t *img;
    lv_obj_t *placeholder;
    int size;
} art_widget_t;

static art_widget_t s_widgets[MAX_WIDGETS];
static int s_widget_count;

static lv_img_dsc_t s_dsc;
static uint8_t *s_pixels;      /* owned; what s_dsc points at */
static uint32_t s_seen_art_version;
static uint32_t s_seen_track_version;
static bool s_track_has_art;

static void apply(art_widget_t *w)
{
    bool show = s_pixels && s_track_has_art;
    if (show) {
        lv_img_set_src(w->img, &s_dsc);
        lv_obj_set_size(w->img, s_dsc.header.w, s_dsc.header.h);
        lv_obj_center(w->img);
        lv_img_set_zoom(w->img, (uint16_t)(w->size * 256 / s_dsc.header.w));
        lv_obj_clear_flag(w->img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(w->placeholder, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(w->img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(w->placeholder, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *ui_art_create(lv_obj_t *parent, int size)
{
    lv_obj_t *box = ui_box(parent);
    lv_obj_set_size(box, size, size);
    lv_obj_set_style_radius(box, size >= 120 ? 16 : 10, 0);
    lv_obj_set_style_clip_corner(box, true, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(UI_COLOR_SURFACE_3), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);

    lv_obj_t *placeholder = ui_label(box, size >= 120 ? UI_FONT_ICON_36 : UI_FONT_ICON_24, 0x5b616b, UI_ICON_MUSIC);
    lv_obj_center(placeholder);

    lv_obj_t *img = lv_img_create(box);
    lv_img_set_antialias(img, true);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);

    if (s_widget_count < MAX_WIDGETS) {
        art_widget_t *w = &s_widgets[s_widget_count++];
        w->img = img;
        w->placeholder = placeholder;
        w->size = size;
        apply(w);
    }
    return box;
}

void ui_art_poll(void)
{
    media_snapshot_t snap;
    media_state_get_snapshot(&snap);

    bool changed = false;
    if (snap.version != s_seen_track_version) {
        s_seen_track_version = snap.version;
        bool has_art = snap.active && snap.has_art;
        changed = has_art != s_track_has_art;
        s_track_has_art = has_art;
    }

    if (snap.art_version != s_seen_art_version) {
        s_seen_art_version = snap.art_version;
        int side = 0;
        uint8_t *pixels = media_state_take_art(&side);
        if (pixels) {
            uint8_t *old = s_pixels;
            s_pixels = pixels;
            s_dsc.header.always_zero = 0;
            s_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
            s_dsc.header.w = side;
            s_dsc.header.h = side;
            s_dsc.data_size = side * side * 2;
            s_dsc.data = s_pixels;
            lv_img_cache_invalidate_src(&s_dsc);
            changed = true;
            /* Safe to free right away: everything runs on the LVGL task and
             * the widgets are re-pointed below before the next redraw. */
            free(old);
        }
    }

    if (changed) {
        for (int i = 0; i < s_widget_count; i++) {
            apply(&s_widgets[i]);
        }
    }
}
