#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WhatsApp desktop notifications and pinned chats from the companion's
 * whatsapp_publisher (deskpanel/whatsapp/feed, deskpanel/whatsapp/contacts).
 * `count` is "new notifications", not the real unread count -- see
 * companion/README.md. Commands go back on deskpanel/whatsapp/cmd.
 */

#define WHATSAPP_MAX_ITEMS    6
#define WHATSAPP_MAX_CONTACTS 6

typedef struct {
    char chat[48];
    char sender[32]; /* "" for direct chats */
    char text[112];  /* "" when previews are disabled */
    char time[8];    /* "14:28" */
} whatsapp_item_t;

typedef struct {
    char id[33];
    char name[32];
} whatsapp_contact_t;

typedef struct {
    uint32_t version;
    bool have_feed;
    int count;
    int item_count;
    whatsapp_item_t items[WHATSAPP_MAX_ITEMS];
    int contact_count;
    whatsapp_contact_t contacts[WHATSAPP_MAX_CONTACTS];
} whatsapp_snapshot_t;

void whatsapp_state_init(void);
void whatsapp_state_get_snapshot(whatsapp_snapshot_t *out);

void whatsapp_state_open_chat(const char *contact_id);
void whatsapp_state_focus(void);

#ifdef __cplusplus
}
#endif
