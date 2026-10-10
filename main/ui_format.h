#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 950 -> "950", 184000 -> "184K", 1250000 -> "1.2M" */
void ui_format_compact(int32_t value, char *out, size_t out_len);

/* Seconds until an event: >= 1 h -> "1:40" (h:mm), else minutes "38". */
void ui_format_countdown_short(int32_t seconds, char *out, size_t out_len);

/* Seconds until an event as "1:39:48" (or "39:48" under an hour). */
void ui_format_countdown_long(int32_t seconds, char *out, size_t out_len);

/* Track time: 102.4 -> "1:42", 3725 -> "1:02:05". */
void ui_format_track_time(float seconds, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
