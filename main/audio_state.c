#include "audio_state.h"
#include "app_mqtt.h"

#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdlib.h>
#include <string.h>

#define TOPIC_AUDIO "deskpanel/audio/state"

static SemaphoreHandle_t s_mutex;
static audio_state_snapshot_t s_snapshot;

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

static void audio_msg_cb(const char *topic, const char *payload, int len, void *ctx)
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

    const cJSON *volume = cJSON_GetObjectItemCaseSensitive(root, "volume");
    const cJSON *muted = cJSON_GetObjectItemCaseSensitive(root, "muted");

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (cJSON_IsNumber(volume)) {
        s_snapshot.volume = volume->valueint;
    }
    if (cJSON_IsBool(muted)) {
        s_snapshot.muted = cJSON_IsTrue(muted);
    }
    s_snapshot.have_data = true;
    xSemaphoreGive(s_mutex);

    cJSON_Delete(root);
}

void audio_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    app_mqtt_subscribe(TOPIC_AUDIO, audio_msg_cb, NULL);
}

void audio_state_get_snapshot(audio_state_snapshot_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snapshot;
    xSemaphoreGive(s_mutex);
}
