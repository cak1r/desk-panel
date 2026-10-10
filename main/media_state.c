#include "media_state.h"
#include "app_mqtt.h"
#include "json_util.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <math.h>
#include <stdio.h>

#define TOPIC_NOWPLAYING "deskpanel/media/nowplaying"
#define TOPIC_ART        "deskpanel/media/art"
#define TOPIC_CMD        "deskpanel/media/cmd"

static const char *TAG = "media_state";

static SemaphoreHandle_t s_mutex;
static media_snapshot_t s_snap;
static int64_t s_received_us; /* when position_s was last reported */

static uint8_t *s_art;        /* newest cover not yet taken by the UI */
static int s_art_size;

static void nowplaying_cb(const char *topic, const char *payload, int len, void *ctx)
{
    cJSON *root = json_parse_payload(payload, len);
    if (!root) {
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_snap.have_data = true;
    s_snap.active = json_bool(root, "active", false);
    char status[16];
    json_copy_str(root, "status", status, sizeof(status));
    s_snap.playing = strcmp(status, "playing") == 0;
    s_snap.can_seek = json_bool(root, "can_seek", false);
    json_copy_str(root, "source", s_snap.source, sizeof(s_snap.source));
    json_copy_str(root, "title", s_snap.title, sizeof(s_snap.title));
    json_copy_str(root, "artist", s_snap.artist, sizeof(s_snap.artist));
    json_copy_str(root, "album", s_snap.album, sizeof(s_snap.album));
    s_snap.position_s = (float)json_num(root, "position_s", 0);
    s_snap.duration_s = (float)json_num(root, "duration_s", 0);
    s_snap.has_art = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(root, "art"));
    s_received_us = esp_timer_get_time();
    s_snap.version++;
    xSemaphoreGive(s_mutex);

    cJSON_Delete(root);
}

static void art_cb(const char *topic, const char *payload, int len, void *ctx)
{
    /* Square RGB565: infer the side from the byte count. */
    int side = (int)lroundf(sqrtf((float)len / 2.0f));
    if (side <= 0 || side > MEDIA_ART_MAX_SIZE || side * side * 2 != len) {
        ESP_LOGW(TAG, "Ignoring %d-byte cover (not a square RGB565 <= %dpx)", len, MEDIA_ART_MAX_SIZE);
        return;
    }

    uint8_t *buf = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (!buf) {
        return;
    }
    memcpy(buf, payload, len);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    free(s_art); /* an untaken older cover is simply superseded */
    s_art = buf;
    s_art_size = side;
    s_snap.art_version++;
    xSemaphoreGive(s_mutex);
}

void media_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    app_mqtt_subscribe(TOPIC_NOWPLAYING, nowplaying_cb, NULL);
    app_mqtt_subscribe(TOPIC_ART, art_cb, NULL);
}

void media_state_get_snapshot(media_snapshot_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snap;
    int64_t received = s_received_us;
    xSemaphoreGive(s_mutex);

    if (out->playing && received > 0) {
        out->position_s += (float)(esp_timer_get_time() - received) / 1e6f;
    }
    if (out->duration_s > 0 && out->position_s > out->duration_s) {
        out->position_s = out->duration_s;
    }
}

uint8_t *media_state_take_art(int *size)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint8_t *art = s_art;
    *size = s_art_size;
    s_art = NULL;
    xSemaphoreGive(s_mutex);
    return art;
}

void media_state_command(const char *action)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"action\":\"%s\"}", action);
    app_mqtt_publish(TOPIC_CMD, buf, 0);
}

void media_state_seek(int delta_s)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"action\":\"seek\",\"delta_s\":%d}", delta_s);
    app_mqtt_publish(TOPIC_CMD, buf, 0);
}
