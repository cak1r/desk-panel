#include "claude_usage.h"
#include "app_mqtt.h"

#include "cJSON.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdlib.h>
#include <string.h>

#define TOPIC_USAGE  "deskpanel/claude/usage"
#define TOPIC_STATUS "deskpanel/companion/status"

static const char *TAG = "claude_usage";

static SemaphoreHandle_t s_mutex;
static claude_usage_snapshot_t s_snapshot;

static char *dup_payload(const char *payload, int len)
{
    char *buf = malloc(len + 1);
    if (!buf) {
        return NULL;
    }
    memcpy(buf, payload, len);
    buf[len] = '\0';
    return buf;
}

static void usage_msg_cb(const char *topic, const char *payload, int len, void *ctx)
{
    (void)topic;
    (void)ctx;

    char *buf = dup_payload(payload, len);
    if (!buf) {
        return;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "Bad JSON on %s", TOPIC_USAGE);
        return;
    }

    const cJSON *requests = cJSON_GetObjectItemCaseSensitive(root, "requests_today");
    const cJSON *tokens = cJSON_GetObjectItemCaseSensitive(root, "tokens_today");

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (cJSON_IsNumber(requests)) {
        s_snapshot.requests_today = requests->valueint;
    }
    if (cJSON_IsNumber(tokens)) {
        s_snapshot.tokens_today = tokens->valueint;
    }
    s_snapshot.have_data = true;
    xSemaphoreGive(s_mutex);

    cJSON_Delete(root);
}

static void status_msg_cb(const char *topic, const char *payload, int len, void *ctx)
{
    (void)topic;
    (void)ctx;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (len == 6 && strncmp(payload, "online", 6) == 0) {
        s_snapshot.companion_online = true;
    } else if (len == 7 && strncmp(payload, "offline", 7) == 0) {
        s_snapshot.companion_online = false;
    }
    xSemaphoreGive(s_mutex);
}

void claude_usage_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_snapshot, 0, sizeof(s_snapshot));

    app_mqtt_subscribe(TOPIC_USAGE, usage_msg_cb, NULL);
    app_mqtt_subscribe(TOPIC_STATUS, status_msg_cb, NULL);
}

void claude_usage_get_snapshot(claude_usage_snapshot_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snapshot;
    xSemaphoreGive(s_mutex);
}
