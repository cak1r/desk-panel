#include "pc_health_state.h"
#include "app_mqtt.h"

#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdlib.h>
#include <string.h>

#define TOPIC_HEALTH "deskpanel/pc/health"

static SemaphoreHandle_t s_mutex;
static pc_health_snapshot_t s_snapshot;

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

static void health_msg_cb(const char *topic, const char *payload, int len, void *ctx)
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
        return;
    }

    const cJSON *cpu = cJSON_GetObjectItemCaseSensitive(root, "cpu");
    const cJSON *ram = cJSON_GetObjectItemCaseSensitive(root, "ram");
    const cJSON *gpu = cJSON_GetObjectItemCaseSensitive(root, "gpu");

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (cJSON_IsNumber(cpu)) {
        s_snapshot.cpu_percent = cpu->valueint;
    }
    if (cJSON_IsNumber(ram)) {
        s_snapshot.ram_percent = ram->valueint;
    }
    if (cJSON_IsNumber(gpu)) {
        s_snapshot.gpu_percent = gpu->valueint;
    }
    s_snapshot.have_data = true;
    xSemaphoreGive(s_mutex);

    cJSON_Delete(root);
}

void pc_health_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.gpu_percent = -1;
    app_mqtt_subscribe(TOPIC_HEALTH, health_msg_cb, NULL);
}

void pc_health_state_get_snapshot(pc_health_snapshot_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snapshot;
    xSemaphoreGive(s_mutex);
}
