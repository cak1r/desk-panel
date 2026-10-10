#include "prayer_times.h"

#include "lvgl.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Location + calculation method for the Aladhan API. Diyanet Isleri
 * Baskanligi (Turkey) is method 13. */
#define PRAYER_CITY    "Istanbul"
#define PRAYER_COUNTRY "Turkey"
#define PRAYER_METHOD  13

#define NVS_NAMESPACE "prayer"
#define NVS_KEY_CACHE "cache"

#define HTTP_RESPONSE_MAX     4096
#define FETCH_RETRY_BACKOFF_S 60

static const char *TAG = "prayer_times";

static const char *const PRAYER_NAMES[PRAYER_COUNT] = {
    "İmsak", "Güneş", "Oğle", "İkindi", "Akşam", "Yatsı",
};

/* Aladhan's "timings" JSON field for each of the above, in order. */
static const char *const ALADHAN_FIELDS[PRAYER_COUNT] = {
    "Imsak", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha",
};

typedef struct {
    int32_t date_stamp; /* YYYYMMDD, 0 = empty */
    prayer_time_t times[PRAYER_COUNT];
} prayer_cache_t;

static SemaphoreHandle_t s_mutex;
static prayer_cache_t s_cache;
static volatile bool s_fetch_in_progress = false;
static time_t s_last_fetch_attempt = 0;

static bool system_time_is_valid(void)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    /* Before SNTP sync completes, the clock reads out near the 1970 epoch. */
    return (tm_now.tm_year + 1900) >= 2024;
}

static int32_t today_date_stamp(void)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    return (int32_t)((tm_now.tm_year + 1900) * 10000 + (tm_now.tm_mon + 1) * 100 + tm_now.tm_mday);
}

static void load_cache_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    prayer_cache_t tmp;
    size_t len = sizeof(tmp);
    if (nvs_get_blob(h, NVS_KEY_CACHE, &tmp, &len) == ESP_OK && len == sizeof(tmp)) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_cache = tmp;
        xSemaphoreGive(s_mutex);
        ESP_LOGI(TAG, "Loaded cached prayer times (date_stamp=%ld)", (long)tmp.date_stamp);
    }
    nvs_close(h);
}

static void save_cache_to_nvs(const prayer_cache_t *cache)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_blob(h, NVS_KEY_CACHE, cache, sizeof(*cache));
    nvs_commit(h);
    nvs_close(h);
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_ctx_t *ctx = (http_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (ctx->len + evt->data_len < ctx->cap) {
            memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
            ctx->len += evt->data_len;
            ctx->buf[ctx->len] = '\0';
        }
    }
    return ESP_OK;
}

static bool parse_time_field(const cJSON *timings, const char *field, prayer_time_t *out, const char *display_name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(timings, field);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }

    int hour = 0, minute = 0;
    if (sscanf(item->valuestring, "%d:%d", &hour, &minute) != 2) {
        return false;
    }

    strncpy(out->name, display_name, sizeof(out->name) - 1);
    out->name[sizeof(out->name) - 1] = '\0';
    out->hour = hour;
    out->minute = minute;
    return true;
}

static bool fetch_and_cache(void)
{
    char url[192];
    snprintf(url, sizeof(url),
             "https://api.aladhan.com/v1/timingsByCity?city=%s&country=%s&method=%d",
             PRAYER_CITY, PRAYER_COUNTRY, PRAYER_METHOD);

    char *response = malloc(HTTP_RESPONSE_MAX);
    if (!response) {
        return false;
    }
    response[0] = '\0';
    http_ctx_t ctx = { .buf = response, .len = 0, .cap = HTTP_RESPONSE_MAX - 1 };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(response);
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "Aladhan fetch failed: err=%s status=%d", esp_err_to_name(err), status);
        free(response);
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    free(response);
    if (!root) {
        ESP_LOGW(TAG, "Aladhan response JSON parse failed");
        return false;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *timings = cJSON_GetObjectItemCaseSensitive(data, "timings");
    if (!cJSON_IsObject(timings)) {
        ESP_LOGW(TAG, "Aladhan response missing data.timings");
        cJSON_Delete(root);
        return false;
    }

    prayer_cache_t cache = { 0 };
    bool all_ok = true;
    for (int i = 0; i < PRAYER_COUNT; i++) {
        if (!parse_time_field(timings, ALADHAN_FIELDS[i], &cache.times[i], PRAYER_NAMES[i])) {
            all_ok = false;
            break;
        }
    }
    cJSON_Delete(root);

    if (!all_ok) {
        ESP_LOGW(TAG, "Aladhan response missing an expected timing field");
        return false;
    }

    cache.date_stamp = today_date_stamp();

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_cache = cache;
    xSemaphoreGive(s_mutex);

    save_cache_to_nvs(&cache);
    ESP_LOGI(TAG, "Prayer times refreshed for %s, %s", PRAYER_CITY, PRAYER_COUNTRY);
    return true;
}

static void fetch_task(void *arg)
{
    (void)arg;
    fetch_and_cache();
    s_fetch_in_progress = false;
    vTaskDelete(NULL);
}

static void maybe_trigger_fetch(void)
{
    if (s_fetch_in_progress || !system_time_is_valid()) {
        return;
    }

    int32_t today = today_date_stamp();

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool stale = (s_cache.date_stamp != today);
    xSemaphoreGive(s_mutex);

    if (!stale) {
        return;
    }

    time_t now = time(NULL);
    if (s_last_fetch_attempt != 0 && (now - s_last_fetch_attempt) < FETCH_RETRY_BACKOFF_S) {
        return;
    }
    s_last_fetch_attempt = now;

    s_fetch_in_progress = true;
    if (xTaskCreate(fetch_task, "prayer_fetch", 8192, NULL, 5, NULL) != pdPASS) {
        s_fetch_in_progress = false;
    }
}

static void tick_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    maybe_trigger_fetch();
}

void prayer_times_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_cache, 0, sizeof(s_cache));

    load_cache_from_nvs();
    maybe_trigger_fetch();

    lv_timer_create(tick_timer_cb, 1000, NULL);
}

void prayer_times_get_snapshot(prayer_snapshot_t *out)
{
    memset(out, 0, sizeof(*out));

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    prayer_cache_t cache = s_cache;
    xSemaphoreGive(s_mutex);

    out->have_data = (cache.date_stamp != 0);
    if (!out->have_data) {
        out->next_index = -1;
        return;
    }
    memcpy(out->times, cache.times, sizeof(out->times));

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    int now_secs = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;

    int next = -1;
    for (int i = 0; i < PRAYER_COUNT; i++) {
        int t_secs = cache.times[i].hour * 3600 + cache.times[i].minute * 60;
        if (t_secs > now_secs) {
            next = i;
            break;
        }
    }

    if (next >= 0) {
        int t_secs = cache.times[next].hour * 3600 + cache.times[next].minute * 60;
        out->next_index = next;
        out->seconds_to_next = t_secs - now_secs;
    } else {
        /* Past Yatsi: estimate tomorrow's Imsak from today's Imsak time,
         * good until the post-midnight refetch corrects it. */
        int t_secs = cache.times[0].hour * 3600 + cache.times[0].minute * 60;
        out->next_index = 0;
        out->seconds_to_next = (24 * 3600 - now_secs) + t_secs;
    }
}
