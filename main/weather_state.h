#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool have_data;
    int temperature_c;
    int feels_like_c;
    int humidity_percent;
    int wind_kmh;
    int weather_code; /* WMO code from Open-Meteo, see weather_state_condition_text() */
} weather_snapshot_t;

/* Short Turkish description for a WMO weather_code (e.g. "Yagmurlu"). */
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
