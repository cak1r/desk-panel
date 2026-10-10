#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Navigation model:
 *   - clock curtain: idle screen, slides up on touch, back down after
 *     IDLE_RETURN_MS of inactivity;
 *   - tileview: passwords tile (left) and the Deck key grid (home);
 *   - pages: full-screen screens opened from Deck keys (or automatically
 *     from the PC's foreground app via deskpanel/profile), each with a
 *     header and an "Ana" button back to the Deck.
 */

typedef enum {
    UI_PAGE_NOWPLAYING,
    UI_PAGE_MIXER,
    UI_PAGE_YOUTUBE,
    UI_PAGE_WHATSAPP,
    UI_PAGE_PRAYER,
    UI_PAGE_WEATHER,
    UI_PAGE_CLAUDE,
    UI_PAGE_HEALTH,
    UI_PAGE_COUNT,
} ui_page_t;

#define UI_SCREEN_W 800
#define UI_SCREEN_H 480
#define UI_HEADER_H 52

void ui_init(void);

void ui_open_page(ui_page_t page);
void ui_close_page(void);

/* Header subtitle of a page (next to its title), for screens that show
 * live context there, e.g. "otomatik · Brave önde". */
lv_obj_t *ui_page_subtitle(ui_page_t page);

#ifdef __cplusplus
}
#endif
