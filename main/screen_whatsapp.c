#include "screen_whatsapp.h"
#include "ui.h"
#include "ui_theme.h"
#include "whatsapp_state.h"

#include <stdio.h>
#include <string.h>

#define VISIBLE_ITEMS 5

typedef struct {
    lv_obj_t *row;
    lv_obj_t *avatar;
    lv_obj_t *chat;
    lv_obj_t *time;
    lv_obj_t *text;
} item_row_t;

typedef struct {
    lv_obj_t *btn;
    lv_obj_t *avatar;
    lv_obj_t *name;
    char id[33];
} contact_btn_t;

static item_row_t s_items[VISIBLE_ITEMS];
static contact_btn_t s_contacts[WHATSAPP_MAX_CONTACTS];
static lv_obj_t *s_empty;
static lv_obj_t *s_no_contacts;
static uint32_t s_seen_version = UINT32_MAX;

/* Up to two initials ("Aile grubu" -> "AG"), UTF-8 aware. */
static void initials(const char *name, char *out, size_t out_len)
{
    size_t n = 0;
    bool word_start = true;
    for (const unsigned char *p = (const unsigned char *)name; *p && n < out_len - 1;) {
        int len = (*p & 0x80) ? ((*p & 0xE0) == 0xC0 ? 2 : (*p & 0xF0) == 0xE0 ? 3 : 4) : 1;
        if (*p == ' ') {
            word_start = true;
        } else if (word_start) {
            if (n + len >= out_len || n >= 6) {
                break;
            }
            memcpy(out + n, p, len);
            n += len;
            word_start = false;
        }
        for (int i = 0; i < len && *p; i++) {
            p++;
        }
    }
    out[n] = '\0';
}

static void focus_cb(lv_event_t *e)
{
    whatsapp_state_focus();
}

static void contact_cb(lv_event_t *e)
{
    contact_btn_t *c = lv_event_get_user_data(e);
    whatsapp_state_open_chat(c->id);
}

static void refresh_cb(lv_timer_t *timer)
{
    whatsapp_snapshot_t snap;
    whatsapp_state_get_snapshot(&snap);
    if (snap.version == s_seen_version) {
        return;
    }
    s_seen_version = snap.version;

    char buf[160];
    if (snap.count > 0) {
        lv_label_set_text_fmt(ui_page_subtitle(UI_PAGE_WHATSAPP), "%d yeni bildirim", snap.count);
    } else {
        lv_label_set_text(ui_page_subtitle(UI_PAGE_WHATSAPP), snap.have_feed ? "yeni bildirim yok" : "");
    }

    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        item_row_t *r = &s_items[i];
        if (i >= snap.item_count) {
            lv_obj_add_flag(r->row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const whatsapp_item_t *it = &snap.items[i];
        initials(it->chat, buf, 12);
        lv_label_set_text(lv_obj_get_child(r->avatar, 0), buf);
        lv_label_set_text(r->chat, it->chat);
        lv_label_set_text(r->time, it->time);
        if (it->sender[0]) {
            snprintf(buf, sizeof(buf), "%s: %s", it->sender, it->text);
        } else {
            snprintf(buf, sizeof(buf), "%s", it->text);
        }
        lv_label_set_text(r->text, buf);
        lv_obj_clear_flag(r->row, LV_OBJ_FLAG_HIDDEN);
    }
    if (snap.item_count == 0) {
        lv_label_set_text(s_empty, snap.have_feed ? "Yeni bildirim yok" : "Companion'dan bildirim bekleniyor…");
        lv_obj_clear_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < WHATSAPP_MAX_CONTACTS; i++) {
        contact_btn_t *c = &s_contacts[i];
        if (i >= snap.contact_count) {
            lv_obj_add_flag(c->btn, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        strncpy(c->id, snap.contacts[i].id, sizeof(c->id) - 1);
        initials(snap.contacts[i].name, buf, 12);
        lv_label_set_text(lv_obj_get_child(c->avatar, 0), buf);
        lv_label_set_text(c->name, snap.contacts[i].name);
        lv_obj_clear_flag(c->btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (snap.contact_count == 0) {
        lv_obj_clear_flag(s_no_contacts, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_no_contacts, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *avatar(lv_obj_t *parent, int size, uint32_t bg, uint32_t fg)
{
    lv_obj_t *a = ui_box(parent);
    lv_obj_set_size(a, size, size);
    lv_obj_set_style_radius(a, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(a, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, 0);
    lv_obj_t *l = ui_label(a, UI_FONT_15, fg, "");
    lv_obj_center(l);
    return a;
}

void screen_whatsapp_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(parent, 16, 0);

    /* Notification list */
    lv_obj_t *list = ui_box(parent);
    lv_obj_set_height(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, 0);

    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        item_row_t *r = &s_items[i];
        r->row = ui_box(list);
        ui_theme_apply_key(r->row);
        lv_obj_set_size(r->row, LV_PCT(100), 68);
        lv_obj_set_style_radius(r->row, 16, 0);
        lv_obj_set_style_pad_ver(r->row, 10, 0);
        lv_obj_set_flex_flow(r->row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r->row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r->row, 12, 0);
        lv_obj_add_event_cb(r->row, focus_cb, LV_EVENT_CLICKED, NULL);

        r->avatar = avatar(r->row, 44, UI_COLOR_SURFACE_3, UI_COLOR_TEXT_2);

        lv_obj_t *col = ui_box(r->row);
        lv_obj_set_height(col, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(col, 2, 0);

        lv_obj_t *top = ui_box(col);
        lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        r->chat = ui_label(top, UI_FONT_15, UI_COLOR_TEXT, "");
        lv_label_set_long_mode(r->chat, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(r->chat, 1);
        r->time = ui_label(top, UI_FONT_13, UI_COLOR_MUTED, "");

        r->text = ui_label(col, UI_FONT_13, UI_COLOR_TEXT_3, "");
        lv_label_set_long_mode(r->text, LV_LABEL_LONG_DOT);
        lv_obj_set_width(r->text, LV_PCT(100));
        lv_obj_add_flag(r->row, LV_OBJ_FLAG_HIDDEN);
    }
    s_empty = ui_label(list, UI_FONT_15, UI_COLOR_MUTED, "");
    lv_obj_set_style_pad_top(s_empty, 16, 0);

    /* Pinned chats + focus button */
    lv_obj_t *side = ui_box(parent);
    lv_obj_set_size(side, 272, LV_PCT(100));
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(side, 10, 0);
    ui_label(side, UI_FONT_13, UI_COLOR_MUTED, "HIZLI SOHBET");

    lv_obj_t *grid = ui_box(side);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);
    for (int i = 0; i < WHATSAPP_MAX_CONTACTS; i++) {
        contact_btn_t *c = &s_contacts[i];
        c->btn = ui_box(grid);
        ui_theme_apply_key(c->btn);
        lv_obj_set_size(c->btn, 131, 82);
        lv_obj_set_style_radius(c->btn, 16, 0);
        lv_obj_set_style_pad_all(c->btn, 8, 0);
        lv_obj_set_flex_flow(c->btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(c->btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(c->btn, 6, 0);
        lv_obj_add_event_cb(c->btn, contact_cb, LV_EVENT_CLICKED, c);
        c->avatar = avatar(c->btn, 36, UI_TINT_WHATSAPP_BG, UI_TINT_WHATSAPP_FG);
        c->name = ui_label(c->btn, UI_FONT_13, UI_COLOR_TEXT_2, "");
        lv_label_set_long_mode(c->name, LV_LABEL_LONG_DOT);
        lv_obj_set_style_max_width(c->name, 115, 0);
        lv_obj_add_flag(c->btn, LV_OBJ_FLAG_HIDDEN);
    }
    s_no_contacts = ui_label(side, UI_FONT_13, UI_COLOR_FAINT,
                             "Sabit sohbetler companion'ın config.py dosyasındaki WHATSAPP_PINNED_CONTACTS listesinden gelir.");
    lv_label_set_long_mode(s_no_contacts, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_no_contacts, LV_PCT(100));

    lv_obj_t *spacer = ui_box(side);
    lv_obj_set_width(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);

    lv_obj_t *focus = ui_box(side);
    ui_theme_apply_key(focus);
    lv_obj_set_size(focus, LV_PCT(100), 56);
    lv_obj_set_style_radius(focus, 16, 0);
    lv_obj_set_style_bg_color(focus, lv_color_hex(UI_COLOR_ACCENT), 0);
    lv_obj_set_style_border_width(focus, 0, 0);
    lv_obj_set_style_pad_all(focus, 0, 0);
    lv_obj_set_flex_flow(focus, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(focus, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(focus, 10, 0);
    lv_obj_add_event_cb(focus, focus_cb, LV_EVENT_CLICKED, NULL);
    ui_label(focus, UI_FONT_ICON_18, UI_COLOR_ON_ACCENT, UI_ICON_EXTERNAL_LINK);
    ui_label(focus, UI_FONT_15, UI_COLOR_ON_ACCENT, "WhatsApp'ı öne getir");

    lv_timer_create(refresh_cb, 1000, NULL);
    refresh_cb(NULL);
}
