#include "hadith_state.h"
#include "app_mqtt.h"
#include "json_util.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TOPIC_HADITH "deskpanel/hadith/today"

static SemaphoreHandle_t s_mutex;
static hadith_snapshot_t s_snap;

static void hadith_cb(const char *topic, const char *payload, int len, void *ctx)
{
    cJSON *root = json_parse_payload(payload, len);
    if (!root) {
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    json_copy_str(root, "text", s_snap.text, sizeof(s_snap.text));
    json_copy_str(root, "source", s_snap.source, sizeof(s_snap.source));
    json_copy_str(root, "grade", s_snap.grade, sizeof(s_snap.grade));
    s_snap.have_data = s_snap.text[0] != '\0';
    s_snap.version++;
    xSemaphoreGive(s_mutex);

    cJSON_Delete(root);
}

void hadith_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    app_mqtt_subscribe(TOPIC_HADITH, hadith_cb, NULL);
}

void hadith_state_get_snapshot(hadith_snapshot_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snap;
    xSemaphoreGive(s_mutex);
}
