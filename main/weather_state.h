#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WEATHER_MAX_DAYS  10
#define WEATHER_MAX_HOURS 6

typedef struct {
    int mday;          /* day of month */
    int wday;          /* 0 = Sunday */
    int weather_code;  /* WMO, -1 if unknown */
    int min_c;
    int max_c;
    int rain_percent;  /* max precipitation probability that day */
} weather_day_t;

typedef struct {
    int hour;          /* local hour 0-23 */
    int rain_percent;
} weather_hour_t;

typedef struct {
    bool have_data;
    int temperature_c;
    int feels_like_c;
    int humidity_percent;
    int wind_kmh;
    int weather_code; /* WMO code from Open-Meteo, see weather_state_condition_text() */
    int day_count;    /* days[0] is today */
    weather_day_t days[WEATHER_MAX_DAYS];
    int hour_count;   /* every 2 hours from now */
    weather_hour_t hours[WEATHER_MAX_HOURS];
} weather_snapshot_t;

/* Short Turkish description for a WMO weather_code (e.g. "Yağmurlu"). */
const char *weather_state_condition_text(int weather_code);

/*
 * Starts the weather data layer: loads any cached reading from NVS
 * immediately, and fetches fresh data from Open-Meteo (no API key needed)
 * once the system clock is known and whenever the cache goes stale (every
 * 30 minutes). Safe to call once, after wifi_time_init().
 */
void weather_state_init(void);

/* Fills `out` with the current snapshot (thread-safe). */
void weather_state_get_snapshot(weather_snapshot_t *out);

#ifdef __cplusplus
}
#endif
