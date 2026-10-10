#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Now playing on the PC, as published by the companion's media_publisher
 * (Windows SMTC): deskpanel/media/nowplaying (JSON) and deskpanel/media/art
 * (raw RGB565 cover). Commands go back on deskpanel/media/cmd.
 */

#define MEDIA_ART_MAX_SIZE 200 /* px; larger covers from the companion are ignored */

typedef struct {
    uint32_t version;   /* bumps on every received update */
    bool have_data;     /* any message received yet */
    bool active;        /* a player session exists */
    bool playing;
    bool can_seek;
    bool has_art;       /* the current track has a cover (else show a placeholder) */
    char source[24];    /* "Brave", "Spotify", ... */
    char title[128];
    char artist[96];
    char album[96];
    float position_s;   /* already extrapolated to "now", see media_state_get_snapshot */
    float duration_s;   /* 0 when unknown */
    uint32_t art_version; /* bumps when a new cover is available via media_state_take_art */
} media_snapshot_t;

void media_state_init(void);

/* Fills `out`; position_s is advanced by the time elapsed since the last
 * update while playing, so a once-a-second UI refresh shows a moving bar. */
void media_state_get_snapshot(media_snapshot_t *out);

/*
 * Hands over the newest cover (RGB565, little endian, size x size). The
 * caller owns the returned buffer (free() it once LVGL no longer draws it).
 * Returns NULL when there's nothing newer than the last take.
 */
uint8_t *media_state_take_art(int *size);

/* "play_pause", "next", "previous", "play", "pause". */
void media_state_command(const char *action);
void media_state_seek(int delta_s);

#ifdef __cplusplus
}
#endif
