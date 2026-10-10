#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool have_data;
    int cpu_percent; /* 0-100 */
    int ram_percent; /* 0-100 */
    int gpu_percent; /* 0-100, -1 if the companion has no GPU reading */
} pc_health_snapshot_t;

/*
 * Subscribes to deskpanel/pc/health over MQTT, expecting JSON
 * {"cpu": <0-100 int>, "ram": <0-100 int>, "gpu": <0-100 int, optional>}
 * as published by the PC companion (psutil/pynvml). Safe to call once, any
 * time after app_mqtt_init().
 */
void pc_health_state_init(void);

/* Fills `out` with the current snapshot (thread-safe). */
void pc_health_state_get_snapshot(pc_health_snapshot_t *out);

#ifdef __cplusplus
}
#endif
