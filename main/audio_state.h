#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool have_data;
    int volume; /* 0-100 */
    bool muted;
} audio_state_snapshot_t;

/*
 * Subscribes to deskpanel/audio/state over MQTT, expecting JSON
 * {"volume": <0-100 int>, "muted": <bool>} as published by the PC
 * companion (pycaw). Safe to call once, any time after app_mqtt_init().
 */
void audio_state_init(void);

/* Fills `out` with the current snapshot (thread-safe). */
void audio_state_get_snapshot(audio_state_snapshot_t *out);

#ifdef __cplusplus
}
#endif
