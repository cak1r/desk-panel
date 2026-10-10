#include "app_mqtt.h"
#include "mqtt_config.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include <string.h>

#define MAX_SUBSCRIPTIONS 8
#define MAX_TOPIC_LEN      64

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
        ESP_LOGW(TAG, "Disconnected from broker");
        break;

    case MQTT_EVENT_DATA:
        for (int i = 0; i < s_sub_count; i++) {
            if ((int)strlen(s_subs[i].topic) == event->topic_len &&
                strncmp(s_subs[i].topic, event->topic, event->topic_len) == 0) {
                s_subs[i].cb(s_subs[i].topic, event->data, event->data_len, s_subs[i].ctx);
                break;
            }
        }
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

bool app_mqtt_is_connected(void)
{
    return s_connected;
}
