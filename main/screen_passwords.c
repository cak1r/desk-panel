#include "screen_passwords.h"
#include "secrets_config.h"
#include "ui_theme.h"
#include "hid_actions.h"

#include <string.h>

#define PIN_LEN 4

static lv_obj_t *s_lock_overlay;
static lv_obj_t *s_content;
static lv_obj_t *s_dots[PIN_LEN];
static lv_obj_t *s_error_label;

static char s_pin_buf[PIN_LEN + 1];
static int s_pin_len;
static bool s_unlocked;

static void update_dots(void)
{
    for (int i = 0; i < PIN_LEN; i++) {
        bool filled = i < s_pin_len;
        lv_obj_set_style_bg_color(s_dots[i], filled ? ui_theme_accent_color() : lv_color_hex(0x2a2f38), 0);
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
    }
}

static void reset_pin(void)
{
    s_pin_len = 0;
    memset(s_pin_buf, 0, sizeof(s_pin_buf));
    update_dots();
}

static void unlock(void)
{
    s_unlocked = true;
    lv_obj_add_flag(s_lock_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
    reset_pin();
}

static void check_pin(void)
{
    if (strcmp(s_pin_buf, LOCK_PIN) == 0) {
        unlock();
    } else {
        lv_label_set_text(s_error_label, "Yanlis PIN");
        lv_obj_clear_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
        reset_pin();
    }
}

static void digit_cb(lv_event_t *e)
{
    char digit = (char)(intptr_t)lv_event_get_user_data(e);

    if (s_pin_len >= PIN_LEN) {
        return;
    }
    s_pin_buf[s_pin_len++] = digit;
    update_dots();

    if (s_pin_len == PIN_LEN) {
        check_pin();
    }
}

static void backspace_cb(lv_event_t *e)
{
    (void)e;
    if (s_pin_len > 0) {
        s_pin_buf[--s_pin_len] = '\0';
        update_dots();
    }
}

static void macro_btn_cb(lv_event_t *e)
{
    const char *text = (const char *)lv_event_get_user_data(e);
    hid_actions_text(text);
}

static lv_obj_t *create_keypad_btn(lv_obj_t *parent, const char *label, const lv_font_t *font, lv_event_cb_t cb,
                                    void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    ui_theme_apply_button(btn);
    lv_obj_set_size(btn, 64, 56);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);

    return btn;
}

static void build_lock_overlay(lv_obj_t *parent)
{
    s_lock_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_lock_overlay);
    lv_obj_set_size(s_lock_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_lock_overlay, 0, 0);
    ui_theme_apply_bg_gradient(s_lock_overlay);
    lv_obj_set_flex_flow(s_lock_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_lock_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_lock_overlay, 16, 0);

    lv_obj_t *lock_icon = lv_label_create(s_lock_overlay);
    lv_obj_set_style_text_font(lock_icon, ui_theme_font_icons(), 0);
    lv_obj_set_style_text_color(lock_icon, ui_theme_accent_color(), 0);
    lv_label_set_text(lock_icon, UI_ICON_LOCK);

    lv_obj_t *title = lv_label_create(s_lock_overlay);
    lv_obj_set_style_text_font(title, ui_theme_font_body(), 0);
    lv_label_set_text(title, "PIN Girin");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t *dots_row = lv_obj_create(s_lock_overlay);
    lv_obj_remove_style_all(dots_row);
    lv_obj_set_size(dots_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots_row, 12, 0);
    for (int i = 0; i < PIN_LEN; i++) {
        lv_obj_t *dot = lv_obj_create(dots_row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 18, 18);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        s_dots[i] = dot;
    }

    s_error_label = lv_label_create(s_lock_overlay);
    lv_obj_set_style_text_font(s_error_label, ui_theme_font_body(), 0);
    lv_obj_set_style_text_color(s_error_label, lv_color_hex(0xe5484d), 0);
    lv_label_set_text(s_error_label, "Yanlis PIN");
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *keypad = lv_obj_create(s_lock_overlay);
    lv_obj_remove_style_all(keypad);
    lv_obj_set_size(keypad, 228, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(keypad, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(keypad, 12, 0);
    lv_obj_set_style_pad_row(keypad, 12, 0);

    for (char d = '1'; d <= '9'; d++) {
        char label[2] = { d, '\0' };
        create_keypad_btn(keypad, label, ui_theme_font_emphasis(), digit_cb, (void *)(intptr_t)d);
    }

    lv_obj_t *spacer = lv_obj_create(keypad);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 64, 56);

    create_keypad_btn(keypad, "0", ui_theme_font_emphasis(), digit_cb, (void *)(intptr_t)'0');
    create_keypad_btn(keypad, UI_ICON_BACKSPACE, ui_theme_font_icons(), backspace_cb, NULL);
}

static void build_content(lv_obj_t *parent)
{
    s_content = lv_obj_create(parent);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_content, 0, 0);
    ui_theme_apply_bg_gradient(s_content);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_content, 16, 0);
    lv_obj_add_flag(s_content, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_content);
    lv_obj_set_style_text_font(title, ui_theme_font_body(), 0);
    lv_label_set_text(title, "Sifreler");
    lv_obj_set_style_text_color(title, lv_color_hex(0x8a8f98), 0);

    for (size_t i = 0; i < MACRO_ENTRY_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(s_content);
        ui_theme_apply_button(btn);
        lv_obj_set_size(btn, 240, 56);
        lv_obj_add_event_cb(btn, macro_btn_cb, LV_EVENT_CLICKED, (void *)MACRO_ENTRIES[i].text);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, ui_theme_font_body(), 0);
        lv_label_set_text(lbl, MACRO_ENTRIES[i].name);
        lv_obj_center(lbl);
    }
}

void screen_passwords_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, ui_theme_bg_color(), 0);

    build_content(parent);
    build_lock_overlay(parent);

    reset_pin();
}

void screen_passwords_lock(void)
{
    if (!s_lock_overlay || !s_unlocked) {
        return;
    }
    s_unlocked = false;
    lv_obj_clear_flag(s_lock_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
    reset_pin();
}
