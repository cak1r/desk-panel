#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_mqtt_msg_cb_t)(const char *topic, const char *payload, int payload_len, void *user_ctx);

/*
 * Connects to the broker configured in mqtt_config.h and dispatches
 * incoming messages to callbacks registered via app_mqtt_subscribe(). A
 * no-op (logs and returns) if MQTT_BROKER_URI is empty. Safe to call once,
 * after wifi_time_init().
 */
void app_mqtt_init(void);

/*
 * Registers a callback for an exact topic match. Subscribes immediately if
 * already connected, and (re)subscribes automatically on every reconnect.
 * Can be called before or after app_mqtt_init().
 */
void app_mqtt_subscribe(const char *topic, app_mqtt_msg_cb_t cb, void *user_ctx);

/* True while the broker connection is currently up. */
bool app_mqtt_is_connected(void);

#ifdef __cplusplus
}
#endif
