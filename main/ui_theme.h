#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_THEME_COLOR_BG     0x0c0e11
#define UI_THEME_COLOR_ACCENT 0x35c5f2 /* vivid sky-blue, echoes the bg_gradient's bright core */

void ui_theme_init(void);

lv_color_t ui_theme_bg_color(void);
lv_color_t ui_theme_accent_color(void);

/* Rounded dark card background, used by stat/prayer-time tiles */
void ui_theme_apply_card(lv_obj_t *obj);

/* Full-screen moody gradient (near-black -> indigo -> dark teal, top to
 * bottom) used as the app's backdrop instead of a flat fill. Pre-rendered
 * once (assets/gen_bg_gradient.py) into a full-width 800x480 image rather
 * than computed live -- a real-time dithered gradient fill was expensive
 * enough to visibly stutter tileview scroll animations, and a narrower
 * tiled strip made the dither pattern's repeat visible as a wave artifact.
 * Apply only to the outermost opaque surfaces (tileview, curtain, detail
 * overlays) -- anything stacked on top of them should stay transparent so
 * it shows through. */
void ui_theme_apply_bg_gradient(lv_obj_t *obj);

/* Dark button with an accent-colored pressed state */
void ui_theme_apply_button(lv_obj_t *obj);

/* Manrope Medium, 163px, digits + ':' only -- the big idle-screen clock. */
const lv_font_t *ui_theme_font_hero(void);

/* Manrope Medium, 32px, full Turkish-aware charset -- emphasis values
 * (prayer countdown, claude stats, volume %). */
const lv_font_t *ui_theme_font_emphasis(void);

/* Manrope Light, 20px, full Turkish-aware charset -- body text and labels. */
const lv_font_t *ui_theme_font_body(void);

/* Tabler outline icon subset, 36px: play/pause, track-prev/next, backspace,
 * volume, lock -- use the UI_ICON_* glyph macros below with this font. */
const lv_font_t *ui_theme_font_icons(void);

/* clang-format off */
#define UI_ICON_PLAYER_PAUSE      "\ued45"
#define UI_ICON_PLAYER_PLAY       "\ued46"
#define UI_ICON_PLAYER_TRACK_NEXT "\ued4b"
#define UI_ICON_PLAYER_TRACK_PREV "\ued4c"
#define UI_ICON_BACKSPACE         "\uea2d"
#define UI_ICON_VOLUME            "\ueb51"
#define UI_ICON_LOCK              "\ueae2"
/* clang-format on */

#ifdef __cplusplus
}
#endif
