#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool have_data;          /* true once at least one usage message arrived */
    bool companion_online;   /* from deskpanel/companion/status ("online"/"offline") */
    int32_t requests_today;
    int32_t tokens_today;
} claude_usage_snapshot_t;

/*
 * Subscribes to the Claude usage topics over MQTT (deskpanel/claude/usage,
 * deskpanel/companion/status). Expects deskpanel/claude/usage to carry a
 * JSON payload: {"requests_today": <int>, "tokens_today": <int>}. Safe to
 * call once, any time after app_mqtt_init().
 */
void claude_usage_init(void);

/* Fills `out` with the current snapshot (thread-safe). */
void claude_usage_get_snapshot(claude_usage_snapshot_t *out);

#ifdef __cplusplus
}
#endif
