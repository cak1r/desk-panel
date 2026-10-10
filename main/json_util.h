#pragma once

/* Small cJSON helpers shared by the MQTT-fed *_state modules. */

#include "cJSON.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Parses a non-NUL-terminated MQTT payload. Caller cJSON_Delete()s. */
static inline cJSON *json_parse_payload(const char *payload, int len)
{
    return cJSON_ParseWithLength(payload, (size_t)len);
}

/* Copies a string member into `out` (truncating, always terminated);
 * leaves "" when missing or not a string. */
static inline void json_copy_str(const cJSON *obj, const char *key, char *out, size_t out_len)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    out[0] = '\0';
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(out, item->valuestring, out_len - 1);
        out[out_len - 1] = '\0';
        /* Don't leave a UTF-8 sequence cut in half at the end. */
        size_t n = strlen(out);
        if (n > 0 && n == out_len - 1) {
            size_t i = n;
            while (i > 0 && ((unsigned char)out[i - 1] & 0xC0) == 0x80) {
                i--;
            }
            if (i > 0 && ((unsigned char)out[i - 1] & 0x80)) {
                unsigned char lead = (unsigned char)out[i - 1];
                size_t need = (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : 4;
                if (n - (i - 1) < need) {
                    out[i - 1] = '\0';
                }
            }
        }
    }
}

static inline double json_num(const cJSON *obj, const char *key, double fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

static inline bool json_bool(const cJSON *obj, const char *key, bool fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
}
