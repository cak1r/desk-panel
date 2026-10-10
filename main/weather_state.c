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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Same location as prayer_times.c (Istanbul). Open-Meteo needs a lat/lon,
 * not a city name. */
#define WEATHER_LAT "41.0082"
#define WEATHER_LON "28.9784"

#define NVS_NAMESPACE "weather"
#define NVS_KEY_CACHE "cache"

#define HTTP_RESPONSE_MAX     8192
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
    int day_count;
    weather_day_t days[WEATHER_MAX_DAYS];
    int hour_count;
    weather_hour_t hours[WEATHER_MAX_HOURS];
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
        case 0: return "Açık";
        case 1: return "Az bulutlu";
        case 2: return "Parçalı bulutlu";
        case 3: return "Kapalı";
        case 45:
        case 48: return "Sisli";
        case 51:
        case 53:
        case 55: return "Çisenti";
        case 56:
        case 57: return "Donan çisenti";
        case 61:
        case 63:
        case 65: return "Yağmurlu";
        case 66:
        case 67: return "Donan yağmur";
        case 71:
        case 73:
        case 75: return "Karlı";
        case 77: return "Kar taneleri";
        case 80:
        case 81:
        case 82: return "Sağanak yağış";
        case 85:
        case 86: return "Kar sağanağı";
        case 95: return "Gök gürültülü";
        case 96:
        case 99: return "Dolulu fırtına";
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

static int array_int(const cJSON *arr, int i, int fallback)
{
    const cJSON *item = cJSON_GetArrayItem(arr, i);
    return cJSON_IsNumber(item) ? round_to_int(item->valuedouble) : fallback;
}

/* "2026-10-10" -> day of month + weekday (0 = Sunday). */
static bool parse_date(const char *iso, int *mday, int *wday)
{
    struct tm tm = { 0 };
    if (!iso || sscanf(iso, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3) {
        return false;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_hour = 12;
    if (mktime(&tm) == (time_t)-1) {
        return false;
    }
    *mday = tm.tm_mday;
    *wday = tm.tm_wday;
    return true;
}

static void parse_daily(const cJSON *daily, weather_cache_t *cache)
{
    const cJSON *dates = cJSON_GetObjectItemCaseSensitive(daily, "time");
    const cJSON *codes = cJSON_GetObjectItemCaseSensitive(daily, "weather_code");
    const cJSON *tmax = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max");
    const cJSON *tmin = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min");
    const cJSON *rain = cJSON_GetObjectItemCaseSensitive(daily, "precipitation_probability_max");

    int n = cJSON_GetArraySize(dates);
    for (int i = 0; i < n && cache->day_count < WEATHER_MAX_DAYS; i++) {
        weather_day_t *d = &cache->days[cache->day_count];
        const cJSON *date = cJSON_GetArrayItem(dates, i);
        if (!cJSON_IsString(date) || !parse_date(date->valuestring, &d->mday, &d->wday)) {
            continue;
        }
        d->weather_code = array_int(codes, i, -1);
        d->max_c = array_int(tmax, i, 0);
        d->min_c = array_int(tmin, i, 0);
        d->rain_percent = array_int(rain, i, 0);
        cache->day_count++;
    }
}

/* Hourly arrays start at the current hour (forecast_hours); keep every
 * HOUR_STEP-th entry so the strip spans the next ~12 hours. */
#define HOUR_STEP 2

static void parse_hourly(const cJSON *hourly, weather_cache_t *cache)
{
    const cJSON *times = cJSON_GetObjectItemCaseSensitive(hourly, "time");
    const cJSON *rain = cJSON_GetObjectItemCaseSensitive(hourly, "precipitation_probability");

    int n = cJSON_GetArraySize(times);
    for (int i = 0; i < n && cache->hour_count < WEATHER_MAX_HOURS; i += HOUR_STEP) {
        const cJSON *t = cJSON_GetArrayItem(times, i);
        int y, mo, d, h, mi;
        if (!cJSON_IsString(t) || sscanf(t->valuestring, "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) != 5) {
            continue;
        }
        cache->hours[cache->hour_count].hour = h;
        cache->hours[cache->hour_count].rain_percent = array_int(rain, i, 0);
        cache->hour_count++;
    }
}

static bool fetch_and_cache(void)
{
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
             "&hourly=precipitation_probability&forecast_days=%d&forecast_hours=%d"
             "&timezone=auto",
             WEATHER_LAT, WEATHER_LON, WEATHER_MAX_DAYS, WEATHER_MAX_HOURS * HOUR_STEP);

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
    parse_daily(cJSON_GetObjectItemCaseSensitive(root, "daily"), &cache);
    parse_hourly(cJSON_GetObjectItemCaseSensitive(root, "hourly"), &cache);
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
    out->day_count = cache.day_count;
    memcpy(out->days, cache.days, sizeof(out->days));
    out->hour_count = cache.hour_count;
    memcpy(out->hours, cache.hours, sizeof(out->hours));
}
