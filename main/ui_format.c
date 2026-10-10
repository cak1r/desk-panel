#include "ui_format.h"

#include <stdio.h>

void ui_format_compact(int32_t value, char *out, size_t out_len)
{
    if (value >= 1000000) {
        snprintf(out, out_len, "%ld.%ldM", (long)(value / 1000000), (long)((value % 1000000) / 100000));
    } else if (value >= 10000) {
        snprintf(out, out_len, "%ldK", (long)(value / 1000));
    } else if (value >= 1000) {
        snprintf(out, out_len, "%ld.%ldK", (long)(value / 1000), (long)((value % 1000) / 100));
    } else {
        snprintf(out, out_len, "%ld", (long)value);
    }
}

void ui_format_countdown_short(int32_t seconds, char *out, size_t out_len)
{
    if (seconds < 0) {
        seconds = 0;
    }
    if (seconds >= 3600) {
        snprintf(out, out_len, "%ld:%02ld", (long)(seconds / 3600), (long)((seconds % 3600) / 60));
    } else {
        /* Round up so "0" only shows once the time has actually come. */
        snprintf(out, out_len, "%ld", (long)((seconds + 59) / 60));
    }
}

void ui_format_countdown_long(int32_t seconds, char *out, size_t out_len)
{
    if (seconds < 0) {
        seconds = 0;
    }
    long h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
    if (h > 0) {
        snprintf(out, out_len, "%ld:%02ld:%02ld", h, m, s);
    } else {
        snprintf(out, out_len, "%ld:%02ld", m, s);
    }
}

void ui_format_track_time(float seconds, char *out, size_t out_len)
{
    long t = seconds > 0 ? (long)seconds : 0;
    if (t >= 3600) {
        snprintf(out, out_len, "%ld:%02ld:%02ld", t / 3600, (t % 3600) / 60, t % 60);
    } else {
        snprintf(out, out_len, "%ld:%02ld", t / 60, t % 60);
    }
}
