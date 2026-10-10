#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRAYER_COUNT     6
#define PRAYER_HIJRI_LEN 40

typedef struct {
    char name[16]; /* display name, e.g. "Imsak" */
    int hour;
    int minute;
} prayer_time_t;

typedef struct {
    bool have_data;             /* true once today's times are known (cache or fresh fetch) */
    prayer_time_t times[PRAYER_COUNT];
    int next_index;             /* index into times[] of the upcoming prayer, or -1 if past Yatsi */
    int32_t seconds_to_next;    /* countdown to times[next_index], or to tomorrow's Imsak estimate */
    char hijri[PRAYER_HIJRI_LEN]; /* "28 Rebiülahir 1448", "" if unknown */
} prayer_snapshot_t;

/*
 * Starts the prayer-time data layer: loads any cached times for today from
 * NVS immediately, and fetches fresh times from the Aladhan API (Istanbul,
 * Diyanet calculation method) once the system clock is known and whenever
 * the cached date goes stale. Safe to call once, after wifi_time_init().
 */
void prayer_times_init(void);

/* Fills `out` with the current snapshot (thread-safe). */
void prayer_times_get_snapshot(prayer_snapshot_t *out);

#ifdef __cplusplus
}
#endif
