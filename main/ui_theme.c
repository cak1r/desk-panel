#include "ui_theme.h"

static bool s_inited = false;
static lv_style_t s_style_bg;
static lv_style_t s_style_surface;
static lv_style_t s_style_key_pressed;

void ui_theme_init(void)
{
    if (s_inited) {
        return;
    }
    s_inited = true;

    lv_style_init(&s_style_bg);
    lv_style_set_bg_color(&s_style_bg, lv_color_hex(UI_COLOR_BG));
    lv_style_set_bg_opa(&s_style_bg, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_bg, lv_color_hex(UI_COLOR_TEXT));

    lv_style_init(&s_style_surface);
    lv_style_set_bg_color(&s_style_surface, lv_color_hex(UI_COLOR_SURFACE));
    lv_style_set_bg_opa(&s_style_surface, LV_OPA_COVER);
    lv_style_set_radius(&s_style_surface, UI_RADIUS_KEY);
    lv_style_set_border_width(&s_style_surface, 1);
    lv_style_set_border_color(&s_style_surface, lv_color_hex(UI_COLOR_BORDER));
    lv_style_set_text_color(&s_style_surface, lv_color_hex(UI_COLOR_TEXT));
    lv_style_set_text_font(&s_style_surface, UI_FONT_15);
    lv_style_set_shadow_width(&s_style_surface, 0);
    lv_style_set_pad_all(&s_style_surface, 14);

    static const lv_style_prop_t trans_props[] = { LV_STYLE_BG_COLOR, LV_STYLE_TRANSFORM_ZOOM, 0 };
    static lv_style_transition_dsc_t trans;
    lv_style_transition_dsc_init(&trans, trans_props, lv_anim_path_ease_out, 120, 0, NULL);
    lv_style_set_transition(&s_style_surface, &trans);

    lv_style_init(&s_style_key_pressed);
    lv_style_set_bg_color(&s_style_key_pressed, lv_color_hex(UI_COLOR_SURFACE_3));
    lv_style_set_transform_zoom(&s_style_key_pressed, 246); /* subtle press "squish", 246/256 */
}

lv_color_t ui_theme_bg_color(void)
{
    return lv_color_hex(UI_COLOR_BG);
}

lv_color_t ui_theme_accent_color(void)
{
    return lv_color_hex(UI_COLOR_ACCENT);
}

void ui_theme_apply_bg(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_style_bg, 0);
}

void ui_theme_apply_surface(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_style_surface, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_theme_apply_key(lv_obj_t *obj)
{
    /* Drop LVGL's default theme (blue buttons, focus outlines) first. */
    lv_obj_remove_style_all(obj);
    ui_theme_apply_surface(obj);
    lv_obj_add_style(obj, &s_style_key_pressed, LV_STATE_PRESSED);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t *ui_box(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    return obj;
}

lv_obj_t *ui_spacer(lv_obj_t *parent, int w, int h)
{
    lv_obj_t *obj = ui_box(parent);
    lv_obj_set_size(obj, w, h);
    return obj;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text ? text : "");
    return label;
}

lv_obj_t *ui_icon_chip(lv_obj_t *parent, int size, uint32_t bg, uint32_t fg, const lv_font_t *icon_font,
                       const char *icon)
{
    lv_obj_t *chip = ui_box(parent);
    lv_obj_set_size(chip, size, size);
    lv_obj_set_style_radius(chip, size >= 40 ? UI_RADIUS_CHIP : 9, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);

    lv_obj_t *glyph = ui_label(chip, icon_font, fg, icon);
    lv_obj_center(glyph);
    return chip;
}

lv_obj_t *ui_progress(lv_obj_t *parent, int width, int height, uint32_t fill_color)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, width, height);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, height / 2, 0);
    lv_obj_set_style_radius(bar, height / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(UI_COLOR_TRACK), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(fill_color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_EVENT_BUBBLE);
    return bar;
}

/* --- Older API ---------------------------------------------------------- */

void ui_theme_apply_card(lv_obj_t *obj)
{
    ui_theme_apply_surface(obj);
}

void ui_theme_apply_button(lv_obj_t *obj)
{
    ui_theme_apply_key(obj);
    lv_obj_set_style_radius(obj, UI_RADIUS_CHIP, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void ui_theme_apply_bg_gradient(lv_obj_t *obj)
{
    ui_theme_apply_bg(obj);
}

const lv_font_t *ui_theme_font_hero(void)
{
    return UI_FONT_NUM_163;
}

const lv_font_t *ui_theme_font_emphasis(void)
{
    return UI_FONT_32;
}

const lv_font_t *ui_theme_font_body(void)
{
    return UI_FONT_20_LIGHT;
}

const lv_font_t *ui_theme_font_icons(void)
{
    return UI_FONT_ICON_36;
}
