#pragma once

#include "lvgl.h"
#include "ui_icons.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Palette from the redesign: a flat near-black ground, slightly lifted
 * surfaces with a hairline border, one sky-blue accent. App tints are
 * dark/bright pairs used for icon chips (bg) and their glyph (fg). */
#define UI_COLOR_BG            0x0c0e11
#define UI_COLOR_SURFACE       0x15181d
#define UI_COLOR_SURFACE_2     0x1c2027
#define UI_COLOR_SURFACE_3     0x23272f
#define UI_COLOR_BORDER        0x232830
#define UI_COLOR_BORDER_STRONG 0x3a414c
#define UI_COLOR_TRACK         0x2a2f38
#define UI_COLOR_TEXT          0xe8eaed
#define UI_COLOR_TEXT_2        0xc9ccd1
#define UI_COLOR_TEXT_3        0xa3a8b0
#define UI_COLOR_MUTED         0x8a8f98
#define UI_COLOR_FAINT         0x6b717b
#define UI_COLOR_ACCENT        0x35c5f2
#define UI_COLOR_ACCENT_BG     0x12262e
#define UI_COLOR_ON_ACCENT     0x071a22
#define UI_COLOR_DANGER        0xff8a8a
#define UI_COLOR_DANGER_BG     0x2a1517
#define UI_COLOR_DANGER_BORDER 0x5a2a2e
#define UI_COLOR_OK            0x3ddc84
#define UI_COLOR_RAIN          0x7fb8ff
#define UI_COLOR_WARM          0xf2b84b

#define UI_TINT_YOUTUBE_BG  0x3a1418
#define UI_TINT_YOUTUBE_FG  0xff6b76
#define UI_TINT_WHATSAPP_BG 0x10301f
#define UI_TINT_WHATSAPP_FG 0x3ddc84
#define UI_TINT_DISCORD_BG  0x1e2240
#define UI_TINT_DISCORD_FG  0x9aa1ff
#define UI_TINT_CODE_BG     0x10263a
#define UI_TINT_CODE_FG     0x5fb0ff
#define UI_TINT_BRAVE_BG    0x3a2210
#define UI_TINT_BRAVE_FG    0xff9a57

/* Kept for older call sites. */
#define UI_THEME_COLOR_BG     UI_COLOR_BG
#define UI_THEME_COLOR_ACCENT UI_COLOR_ACCENT

#define UI_RADIUS_KEY  18
#define UI_RADIUS_CHIP 12

/* Fonts (Manrope; see assets/gen_fonts.py). *_num fonts only carry digits
 * and ":%°-+.,×". */
LV_FONT_DECLARE(manrope_medium_13);
LV_FONT_DECLARE(manrope_medium_15);
LV_FONT_DECLARE(manrope_medium_18);
LV_FONT_DECLARE(manrope_light_20);
LV_FONT_DECLARE(manrope_medium_26);
LV_FONT_DECLARE(manrope_medium_32);
LV_FONT_DECLARE(manrope_medium_60_num);
LV_FONT_DECLARE(manrope_medium_163_num);
LV_FONT_DECLARE(tabler_icons_18);
LV_FONT_DECLARE(tabler_icons_24);
LV_FONT_DECLARE(tabler_icons_36);

#define UI_FONT_13       (&manrope_medium_13)
#define UI_FONT_15       (&manrope_medium_15)
#define UI_FONT_18       (&manrope_medium_18)
#define UI_FONT_20_LIGHT (&manrope_light_20)
#define UI_FONT_26       (&manrope_medium_26)
#define UI_FONT_32       (&manrope_medium_32)
#define UI_FONT_NUM_60   (&manrope_medium_60_num)
#define UI_FONT_NUM_163  (&manrope_medium_163_num)
#define UI_FONT_ICON_18  (&tabler_icons_18)
#define UI_FONT_ICON_24  (&tabler_icons_24)
#define UI_FONT_ICON_36  (&tabler_icons_36)

void ui_theme_init(void);

lv_color_t ui_theme_bg_color(void);
lv_color_t ui_theme_accent_color(void);

/* Opaque flat background for full-screen surfaces (screen, tileview,
 * overlays, the clock curtain). */
void ui_theme_apply_bg(lv_obj_t *obj);

/* Lifted rounded panel with a hairline border (cards, list rows). */
void ui_theme_apply_surface(lv_obj_t *obj);

/* Surface that reacts to touch: darker fill + slight shrink while pressed. */
void ui_theme_apply_key(lv_obj_t *obj);

/* Plain layout container: no style, transparent, not scrollable, clicks
 * pass through to the parent unless the caller makes it clickable. */
lv_obj_t *ui_box(lv_obj_t *parent);

/* Fixed-size empty box for spacing (LVGL 8 has no margins). */
lv_obj_t *ui_spacer(lv_obj_t *parent, int w, int h);

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text);

/* Rounded square with a tinted fill and a centered icon glyph. */
lv_obj_t *ui_icon_chip(lv_obj_t *parent, int size, uint32_t bg, uint32_t fg, const lv_font_t *icon_font,
                       const char *icon);

/* Thin rounded progress bar (track + fill), value 0-100. */
lv_obj_t *ui_progress(lv_obj_t *parent, int width, int height, uint32_t fill_color);

/* --- Older API, mapped onto the new look --------------------------------- */
void ui_theme_apply_card(lv_obj_t *obj);         /* = ui_theme_apply_surface */
void ui_theme_apply_button(lv_obj_t *obj);       /* = ui_theme_apply_key */
void ui_theme_apply_bg_gradient(lv_obj_t *obj);  /* = ui_theme_apply_bg */
const lv_font_t *ui_theme_font_hero(void);       /* 163px digits */
const lv_font_t *ui_theme_font_emphasis(void);   /* 32px */
const lv_font_t *ui_theme_font_body(void);       /* 20px light */
const lv_font_t *ui_theme_font_icons(void);      /* 36px icons */

#ifdef __cplusplus
}
#endif
