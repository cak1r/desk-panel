#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * "Hadith of the day" pushed by the PC companion on deskpanel/hadith/today
 * as {"text": "...", "source": "Buhârî, 1", "grade": "Sahih"}. The prayer
 * screen only shows the card once a message has arrived -- the panel never
 * makes one up.
 */

typedef struct {
    uint32_t version;
    bool have_data;
    char text[640];
    char source[96];
    char grade[32];
} hadith_snapshot_t;

void hadith_state_init(void);
void hadith_state_get_snapshot(hadith_snapshot_t *out);

#ifdef __cplusplus
}
#endif
