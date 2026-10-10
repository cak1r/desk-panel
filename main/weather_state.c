#include "weather_state.h"

#include "lvgl.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Same location as prayer_times.c (Istanbul). Open-Meteo needs a lat/lon,
 * not a city name. */
#define WEATHER_LAT "41.0082"
#define WEATHER_LON "28.9784"

#define NVS_NAMESPACE "weather"
#define NVS_KEY_CACHE "cache"

#define HTTP_RESPONSE_MAX     2048
#define WEATHER_REFRESH_S     (30 * 60)
#define FETCH_RETRY_BACKOFF_S 60

static const char *TAG = "weather_state";

typedef struct {
    time_t fetched_at; /* 0 = empty */
    int temperature_c;
    int feels_like_c;
    int humidity_percent;
    int wind_kmh;
    int weather_code;
} weather_cache_t;

static SemaphoreHandle_t s_mutex;
static weather_cache_t s_cache;
static volatile bool s_fetch_in_progress = false;
static time_t s_last_fetch_attempt = 0;

static bool system_time_is_valid(void)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    return (tm_now.tm_year + 1900) >= 2024;
}

const char *weather_state_condition_text(int weather_code)
{
    switch (weather_code) {
        case 0: return "Acik";
        case 1: return "Az Bulutlu";
        case 2: return "Parcali Bulutlu";
        case 3: return "Kapali";
        case 45:
        case 48: return "Sisli";
        case 51:
        case 53:
        case 55: return "Cisenti";
        case 56:
        case 57: return "Donan Cisenti";
        case 61:
        case 63:
        case 65: return "Yagmurlu";
        case 66:
        case 67: return "Donan Yagmur";
        case 71:
        case 73:
        case 75: return "Karli";
        case 77: return "Kar Tanesi";
        case 80:
        case 81:
        case 82: return "Saganak Yagis";
        case 85:
        case 86: return "Kar Saganagi";
        case 95: return "Gok Gurultulu";
        case 96:
        case 99: return "Dolu ile Firtina";
        default: return "Bilinmiyor";
    }
}

static void load_cache_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    weather_cache_t tmp;
    size_t len = sizeof(tmp);
    if (nvs_get_blob(h, NVS_KEY_CACHE, &tmp, &len) == ESP_OK && len == sizeof(tmp)) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_cache = tmp;
        xSemaphoreGive(s_mutex);
        ESP_LOGI(TAG, "Loaded cached weather (fetched_at=%ld)", (long)tmp.fetched_at);
    }
    nvs_close(h);
}

static void save_cache_to_nvs(const weather_cache_t *cache)
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

static int round_to_int(double v)
{
    return (int)(v >= 0.0 ? v + 0.5 : v - 0.5);
}

static bool fetch_and_cache(void)
{
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
             "&timezone=auto",
             WEATHER_LAT, WEATHER_LON);

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
        ESP_LOGW(TAG, "Open-Meteo fetch failed: err=%s status=%d", esp_err_to_name(err), status);
        free(response);
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    free(response);
    if (!root) {
        ESP_LOGW(TAG, "Open-Meteo response JSON parse failed");
        return false;
    }

    const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    const cJSON *temp = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
    const cJSON *feels = cJSON_GetObjectItemCaseSensitive(current, "apparent_temperature");
    const cJSON *humidity = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
    const cJSON *wind = cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");

    if (!cJSON_IsNumber(temp) || !cJSON_IsNumber(feels) || !cJSON_IsNumber(humidity) ||
        !cJSON_IsNumber(code) || !cJSON_IsNumber(wind)) {
        ESP_LOGW(TAG, "Open-Meteo response missing an expected field");
        cJSON_Delete(root);
        return false;
    }

    weather_cache_t cache = {
        .fetched_at = time(NULL),
        .temperature_c = round_to_int(temp->valuedouble),
        .feels_like_c = round_to_int(feels->valuedouble),
        .humidity_percent = round_to_int(humidity->valuedouble),
        .wind_kmh = round_to_int(wind->valuedouble),
        .weather_code = code->valueint,
    };
    cJSON_Delete(root);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_cache = cache;
    xSemaphoreGive(s_mutex);

    save_cache_to_nvs(&cache);
    ESP_LOGI(TAG, "Weather refreshed: %dC (feels %dC), code=%d", cache.temperature_c, cache.feels_like_c,
             cache.weather_code);
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

    time_t now = time(NULL);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool stale = (now - s_cache.fetched_at) >= WEATHER_REFRESH_S;
    xSemaphoreGive(s_mutex);

    if (!stale) {
        return;
    }

    if (s_last_fetch_attempt != 0 && (now - s_last_fetch_attempt) < FETCH_RETRY_BACKOFF_S) {
        return;
    }
    s_last_fetch_attempt = now;

    s_fetch_in_progress = true;
    if (xTaskCreate(fetch_task, "weather_fetch", 8192, NULL, 5, NULL) != pdPASS) {
        s_fetch_in_progress = false;
    }
}

static void tick_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    maybe_trigger_fetch();
}

void weather_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_cache, 0, sizeof(s_cache));

    load_cache_from_nvs();
    maybe_trigger_fetch();

    lv_timer_create(tick_timer_cb, 5000, NULL);
}

void weather_state_get_snapshot(weather_snapshot_t *out)
{
    memset(out, 0, sizeof(*out));

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    weather_cache_t cache = s_cache;
    xSemaphoreGive(s_mutex);

    out->have_data = (cache.fetched_at != 0);
    if (!out->have_data) {
        return;
    }

    out->temperature_c = cache.temperature_c;
    out->feels_like_c = cache.feels_like_c;
    out->humidity_percent = cache.humidity_percent;
    out->wind_kmh = cache.wind_kmh;
    out->weather_code = cache.weather_code;
}
