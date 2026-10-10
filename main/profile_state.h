#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Which panel profile matches the PC's foreground app, from the
 * companion's foreground_publisher (deskpanel/profile):
 * "youtube", "ytmusic", "whatsapp", or "" for none.
 */
void profile_state_init(void);

/* Copies the current profile into `out`; returns its version (bumps on change). */
uint32_t profile_state_get(char *out, int out_len);

#ifdef __cplusplus
}
#endif
