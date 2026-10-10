#include "mixer_state.h"
#include "app_mqtt.h"
#include "json_util.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdio.h>

#define TOPIC_STATE "deskpanel/mixer/state"
#define TOPIC_CMD   "deskpanel/mixer/cmd"

static SemaphoreHandle_t s_mutex;
static mixer_snapshot_t s_snap;

static void parse_endpoint(const cJSON *obj, mixer_endpoint_t *out)
{
    out->present = cJSON_IsObject(obj);
    if (!out->present) {
        return;
    }
    json_copy_str(obj, "name", out->name, sizeof(out->name));
    out->volume = (int)json_num(obj, "volume", 0);
    out->muted = json_bool(obj, "muted", false);
}

static void state_cb(const char *topic, const char *payload, int len, void *ctx)
{
    cJSON *root = json_parse_payload(payload, len);
    if (!root) {
        return;
    }

    mixer_snapshot_t next = { 0 };
    next.have_data = true;
    parse_endpoint(cJSON_GetObjectItemCaseSensitive(root, "output"), &next.output);
    parse_endpoint(cJSON_GetObjectItemCaseSensitive(root, "mic"), &next.mic);

    const cJSON *app;
    cJSON_ArrayForEach(app, cJSON_GetObjectItemCaseSensitive(root, "apps")) {
        if (next.app_count >= MIXER_MAX_APPS) {
            break;
        }
        mixer_app_t *a = &next.apps[next.app_count];
        json_copy_str(app, "id", a->id, sizeof(a->id));
        json_copy_str(app, "name", a->name, sizeof(a->name));
        a->volume = (int)json_num(app, "volume", 0);
        a->muted = json_bool(app, "muted", false);
        if (a->id[0]) {
            next.app_count++;
        }
    }
    cJSON_Delete(root);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    next.version = s_snap.version + 1;
    s_snap = next;
    xSemaphoreGive(s_mutex);
}

void mixer_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    app_mqtt_subscribe(TOPIC_STATE, state_cb, NULL);
}

void mixer_state_get_snapshot(mixer_snapshot_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snap;
    xSemaphoreGive(s_mutex);
}

void mixer_state_set_volume(const char *target, int volume)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"target\":\"%s\",\"volume\":%d}", target, volume);
    app_mqtt_publish(TOPIC_CMD, buf, 0);
}

void mixer_state_set_mute(const char *target, bool muted)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"target\":\"%s\",\"mute\":%s}", target, muted ? "true" : "false");
    app_mqtt_publish(TOPIC_CMD, buf, 0);
}
