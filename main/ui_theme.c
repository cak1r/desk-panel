#include "ui_theme.h"

LV_FONT_DECLARE(manrope_medium_163_num);
LV_FONT_DECLARE(manrope_medium_32);
LV_FONT_DECLARE(manrope_light_20);
LV_FONT_DECLARE(tabler_icons_36);
LV_IMG_DECLARE(bg_gradient);

static bool s_inited = false;
static lv_style_t s_style_card;
static lv_style_t s_style_btn;
static lv_style_t s_style_btn_pressed;
static lv_style_t s_style_bg_gradient;

void ui_theme_init(void)
{
    if (s_inited) {
        return;
    }
    s_inited = true;

    /* Frosted-glass look: semi-transparent, purple-tinted panels (instead of
     * flat opaque near-black) so the radial background bleeds through
     * softly, plus a faint light edge to read as a raised surface. */
    lv_style_init(&s_style_card);
    lv_style_set_bg_color(&s_style_card, lv_color_hex(0x201a38));
    lv_style_set_bg_opa(&s_style_card, LV_OPA_70);
    lv_style_set_radius(&s_style_card, 16);
    lv_style_set_border_width(&s_style_card, 1);
    lv_style_set_border_color(&s_style_card, lv_color_hex(0xffffff));
    lv_style_set_border_opa(&s_style_card, LV_OPA_10);
    lv_style_set_pad_all(&s_style_card, 16);

    static const lv_style_prop_t trans_props[] = { LV_STYLE_BG_COLOR, LV_STYLE_TRANSFORM_ZOOM, 0 };
    static lv_style_transition_dsc_t trans;
    lv_style_transition_dsc_init(&trans, trans_props, lv_anim_path_ease_out, 150, 0, NULL);

    lv_style_init(&s_style_btn);
    lv_style_set_bg_color(&s_style_btn, lv_color_hex(0x261e42));
    lv_style_set_bg_opa(&s_style_btn, LV_OPA_70);
    lv_style_set_radius(&s_style_btn, 12);
    lv_style_set_border_width(&s_style_btn, 1);
    lv_style_set_border_color(&s_style_btn, lv_color_hex(0xffffff));
    lv_style_set_border_opa(&s_style_btn, LV_OPA_10);
    lv_style_set_text_color(&s_style_btn, lv_color_hex(0xe6e8eb));
    lv_style_set_text_font(&s_style_btn, &manrope_light_20);
    lv_style_set_transition(&s_style_btn, &trans);

    lv_style_init(&s_style_btn_pressed);
    lv_style_set_bg_color(&s_style_btn_pressed, lv_color_hex(UI_THEME_COLOR_ACCENT));
    lv_style_set_text_color(&s_style_btn_pressed, lv_color_hex(UI_THEME_COLOR_BG));
    lv_style_set_transform_zoom(&s_style_btn_pressed, 240); /* subtle press "squish", 240/256 */

    /* Was a live LV_DITHER_ERR_DIFF gradient fill, recomputed on every redraw
     * (expensive on this MCU, especially during tileview scroll animations).
     * Pre-rendered once instead (assets/gen_bg_gradient.py) into a full-width
     * 800x480 dithered image -- same look, just a cheap blit now. A narrower
     * tiled strip was tried first but the repeating dither pattern showed up
     * as a visible nested wave/curve artifact, so this is full width, untiled. */
    lv_style_init(&s_style_bg_gradient);
    lv_style_set_bg_img_src(&s_style_bg_gradient, &bg_gradient);
    lv_style_set_bg_img_opa(&s_style_bg_gradient, LV_OPA_COVER);
}

lv_color_t ui_theme_bg_color(void)
{
    return lv_color_hex(UI_THEME_COLOR_BG);
}

lv_color_t ui_theme_accent_color(void)
{
    return lv_color_hex(UI_THEME_COLOR_ACCENT);
}

void ui_theme_apply_card(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_style_card, 0);
}

void ui_theme_apply_button(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_style_btn, 0);
    lv_obj_add_style(obj, &s_style_btn_pressed, LV_STATE_PRESSED);
}

void ui_theme_apply_bg_gradient(lv_obj_t *obj)
{
    lv_obj_add_style(obj, &s_style_bg_gradient, 0);
}

const lv_font_t *ui_theme_font_hero(void)
{
    return &manrope_medium_163_num;
}

const lv_font_t *ui_theme_font_emphasis(void)
{
    return &manrope_medium_32;
}

const lv_font_t *ui_theme_font_body(void)
{
    return &manrope_light_20;
}

const lv_font_t *ui_theme_font_icons(void)
{
    return &tabler_icons_36;
}
