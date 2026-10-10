#include "profile_state.h"
#include "app_mqtt.h"
#include "json_util.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TOPIC_PROFILE "deskpanel/profile"

static SemaphoreHandle_t s_mutex;
static char s_profile[24];
static uint32_t s_version;

static void profile_cb(const char *topic, const char *payload, int len, void *ctx)
{
    cJSON *root = json_parse_payload(payload, len);
    if (!root) {
        return;
    }
    char profile[sizeof(s_profile)];
    json_copy_str(root, "profile", profile, sizeof(profile)); /* null -> "" */
    cJSON_Delete(root);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (strcmp(profile, s_profile) != 0) {
        strcpy(s_profile, profile);
        s_version++;
    }
    xSemaphoreGive(s_mutex);
}

void profile_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    app_mqtt_subscribe(TOPIC_PROFILE, profile_cb, NULL);
}

uint32_t profile_state_get(char *out, int out_len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    strncpy(out, s_profile, out_len - 1);
    out[out_len - 1] = '\0';
    uint32_t v = s_version;
    xSemaphoreGive(s_mutex);
    return v;
}
