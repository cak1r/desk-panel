#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Per-app volumes from the companion's mixer_publisher
 * (deskpanel/mixer/state); commands go back on deskpanel/mixer/cmd.
 * Special targets: MIXER_TARGET_OUTPUT (default speakers) and
 * MIXER_TARGET_MIC (default microphone).
 */

#define MIXER_MAX_APPS    8
#define MIXER_TARGET_OUTPUT "@output"
#define MIXER_TARGET_MIC    "@mic"

typedef struct {
    bool present;
    char name[64];
    int volume; /* 0-100 */
    bool muted;
} mixer_endpoint_t;

typedef struct {
    char id[40];   /* process name, e.g. "brave.exe" -- the command target */
    char name[24]; /* display name, e.g. "Brave" */
    int volume;
    bool muted;
} mixer_app_t;

typedef struct {
    uint32_t version;
    bool have_data;
    mixer_endpoint_t output;
    mixer_endpoint_t mic;
    int app_count;
    mixer_app_t apps[MIXER_MAX_APPS];
} mixer_snapshot_t;

void mixer_state_init(void);
void mixer_state_get_snapshot(mixer_snapshot_t *out);

void mixer_state_set_volume(const char *target, int volume);
void mixer_state_set_mute(const char *target, bool muted);

#ifdef __cplusplus
}
#endif
