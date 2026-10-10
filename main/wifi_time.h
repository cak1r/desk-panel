#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Brings up WiFi STA (credentials from wifi_credentials.h) and starts an
 * SNTP sync once connected, so the system clock (and therefore the clock
 * tile) reflects real time. Non-blocking -- connection/sync happen in the
 * background via the default event loop.
 */
void wifi_time_init(void);

#ifdef __cplusplus
}
#endif
