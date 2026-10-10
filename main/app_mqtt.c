#include "app_mqtt.h"
#include "mqtt_config.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include <stdlib.h>
#include <string.h>

#define MAX_SUBSCRIPTIONS 16
#define MAX_TOPIC_LEN      64

/* Largest message reassembled from fragments (cover art is 56 KB at the
 * companion's default 168px); anything bigger is dropped. */
#define MAX_PAYLOAD_LEN (128 * 1024)

typedef struct {
    char topic[MAX_TOPIC_LEN];
    app_mqtt_msg_cb_t cb;
    void *ctx;
} subscription_t;

static const char *TAG = "app_mqtt";

static esp_mqtt_client_handle_t s_client;
static subscription_t s_subs[MAX_SUBSCRIPTIONS];
static int s_sub_count = 0;
static volatile bool s_connected = false;

/* esp-mqtt hands messages larger than its receive buffer over as several
 * MQTT_EVENT_DATA events; only the first carries the topic. Events come
 * from the single MQTT task, so one in-flight message is all we track. */
static struct {
    char *buf;
    int total;
    int received;
    int sub;
} s_frag = { .sub = -1 };

static int find_subscription(const char *topic, int topic_len)
{
    for (int i = 0; i < s_sub_count; i++) {
        if ((int)strlen(s_subs[i].topic) == topic_len && strncmp(s_subs[i].topic, topic, topic_len) == 0) {
            return i;
        }
    }
    return -1;
}

static void frag_reset(void)
{
    free(s_frag.buf);
    s_frag.buf = NULL;
    s_frag.total = s_frag.received = 0;
    s_frag.sub = -1;
}

static void handle_data(esp_mqtt_event_handle_t event)
{
    /* Whole message in one event: dispatch straight from esp-mqtt's buffer. */
    if (event->current_data_offset == 0 && event->data_len == event->total_data_len) {
        int i = find_subscription(event->topic, event->topic_len);
        if (i >= 0) {
            s_subs[i].cb(s_subs[i].topic, event->data, event->data_len, s_subs[i].ctx);
        }
        return;
    }

    if (event->current_data_offset == 0) {
        frag_reset();
        s_frag.sub = find_subscription(event->topic, event->topic_len);
        if (s_frag.sub < 0) {
            return;
        }
        if (event->total_data_len > MAX_PAYLOAD_LEN) {
            ESP_LOGW(TAG, "Dropping %d-byte message on %s (max %d)", event->total_data_len,
                     s_subs[s_frag.sub].topic, MAX_PAYLOAD_LEN);
            frag_reset();
            return;
        }
        s_frag.buf = heap_caps_malloc(event->total_data_len, MALLOC_CAP_SPIRAM);
        if (!s_frag.buf) {
            ESP_LOGE(TAG, "No memory for %d-byte message", event->total_data_len);
            frag_reset();
            return;
        }
        s_frag.total = event->total_data_len;
    }

    /* A continuation for a message we're not collecting (unsubscribed
     * topic, oversized, or its first chunk was lost). */
    if (!s_frag.buf || event->total_data_len != s_frag.total ||
        event->current_data_offset != s_frag.received ||
        event->current_data_offset + event->data_len > s_frag.total) {
        return;
    }

    memcpy(s_frag.buf + s_frag.received, event->data, event->data_len);
    s_frag.received += event->data_len;

    if (s_frag.received == s_frag.total) {
        subscription_t *sub = &s_subs[s_frag.sub];
        sub->cb(sub->topic, s_frag.buf, s_frag.total, sub->ctx);
        frag_reset();
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "Connected to broker");
        for (int i = 0; i < s_sub_count; i++) {
            esp_mqtt_client_subscribe(s_client, s_subs[i].topic, 1);
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        frag_reset();
        ESP_LOGW(TAG, "Disconnected from broker");
        break;

    case MQTT_EVENT_DATA:
        handle_data(event);
        break;

    default:
        break;
    }
}

void app_mqtt_init(void)
{
    if (strlen(MQTT_BROKER_URI) == 0) {
        ESP_LOGW(TAG, "MQTT_BROKER_URI is empty, skipping MQTT (fill in main/mqtt_config.h)");
        return;
    }

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

void app_mqtt_subscribe(const char *topic, app_mqtt_msg_cb_t cb, void *user_ctx)
{
    if (s_sub_count >= MAX_SUBSCRIPTIONS) {
        ESP_LOGE(TAG, "Too many MQTT subscriptions (max %d)", MAX_SUBSCRIPTIONS);
        return;
    }

    subscription_t *sub = &s_subs[s_sub_count++];
    strncpy(sub->topic, topic, sizeof(sub->topic) - 1);
    sub->topic[sizeof(sub->topic) - 1] = '\0';
    sub->cb = cb;
    sub->ctx = user_ctx;

    if (s_connected && s_client) {
        esp_mqtt_client_subscribe(s_client, topic, 1);
    }
}

bool app_mqtt_publish(const char *topic, const char *payload, int payload_len)
{
    if (!s_client || !s_connected) {
        return false;
    }
    /* QoS 0: commands are only meaningful right now; a stale "next track"
     * replayed after a reconnect would be worse than a dropped one. */
    return esp_mqtt_client_publish(s_client, topic, payload, payload_len, 0, 0) >= 0;
}

bool app_mqtt_is_connected(void)
{
    return s_connected;
}
