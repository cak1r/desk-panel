#include "whatsapp_state.h"
#include "app_mqtt.h"
#include "json_util.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdio.h>

#define TOPIC_FEED     "deskpanel/whatsapp/feed"
#define TOPIC_CONTACTS "deskpanel/whatsapp/contacts"
#define TOPIC_CMD      "deskpanel/whatsapp/cmd"

static SemaphoreHandle_t s_mutex;
static whatsapp_snapshot_t s_snap;

static void feed_cb(const char *topic, const char *payload, int len, void *ctx)
{
    cJSON *root = json_parse_payload(payload, len);
    if (!root) {
        return;
    }

    whatsapp_item_t items[WHATSAPP_MAX_ITEMS];
    int n = 0;
    const cJSON *it;
    cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(root, "items")) {
        if (n >= WHATSAPP_MAX_ITEMS) {
            break;
        }
        json_copy_str(it, "chat", items[n].chat, sizeof(items[n].chat));
        json_copy_str(it, "sender", items[n].sender, sizeof(items[n].sender));
        json_copy_str(it, "text", items[n].text, sizeof(items[n].text));
        json_copy_str(it, "time", items[n].time, sizeof(items[n].time));
        n++;
    }
    int count = (int)json_num(root, "count", n);
    cJSON_Delete(root);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_snap.have_feed = true;
    s_snap.count = count;
    s_snap.item_count = n;
    memcpy(s_snap.items, items, sizeof(items[0]) * n);
    s_snap.version++;
    xSemaphoreGive(s_mutex);
}

static void contacts_cb(const char *topic, const char *payload, int len, void *ctx)
{
    cJSON *root = json_parse_payload(payload, len);
    if (!root) {
        return;
    }

    whatsapp_contact_t contacts[WHATSAPP_MAX_CONTACTS];
    int n = 0;
    const cJSON *c;
    cJSON_ArrayForEach(c, root) {
        if (n >= WHATSAPP_MAX_CONTACTS) {
            break;
        }
        json_copy_str(c, "id", contacts[n].id, sizeof(contacts[n].id));
        json_copy_str(c, "name", contacts[n].name, sizeof(contacts[n].name));
        if (contacts[n].id[0]) {
            n++;
        }
    }
    cJSON_Delete(root);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_snap.contact_count = n;
    memcpy(s_snap.contacts, contacts, sizeof(contacts[0]) * n);
    s_snap.version++;
    xSemaphoreGive(s_mutex);
}

void whatsapp_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    app_mqtt_subscribe(TOPIC_FEED, feed_cb, NULL);
    app_mqtt_subscribe(TOPIC_CONTACTS, contacts_cb, NULL);
}

void whatsapp_state_get_snapshot(whatsapp_snapshot_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snap;
    xSemaphoreGive(s_mutex);
}

void whatsapp_state_open_chat(const char *contact_id)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"action\":\"open_chat\",\"contact\":\"%s\"}", contact_id);
    app_mqtt_publish(TOPIC_CMD, buf, 0);
}

void whatsapp_state_focus(void)
{
    app_mqtt_publish(TOPIC_CMD, "{\"action\":\"focus\"}", 0);
}
