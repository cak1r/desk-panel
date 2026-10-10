/*
 * Host-side unit tests for firmware logic that doesn't touch hardware or
 * LVGL: display formatting (ui_format.c) and the MQTT JSON helpers
 * (json_util.h). Build and run with `make -C test/host` (needs IDF_PATH
 * for cJSON).
 */

#include "ui_format.h"
#include "json_util.h"

#include <stdio.h>
#include <string.h>

static int s_failures;
static int s_checks;

#define CHECK_STR(expr, expected)                                                                  \
    do {                                                                                           \
        s_checks++;                                                                                \
        const char *got_ = (expr);                                                                 \
        if (strcmp(got_, (expected)) != 0) {                                                       \
            fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, (expected), \
                    got_);                                                                         \
            s_failures++;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK(cond)                                                       \
    do {                                                                  \
        s_checks++;                                                       \
        if (!(cond)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            s_failures++;                                                 \
        }                                                                 \
    } while (0)

static char buf[64];

static const char *compact(int32_t v)
{
    ui_format_compact(v, buf, sizeof(buf));
    return buf;
}

static const char *cd_short(int32_t s)
{
    ui_format_countdown_short(s, buf, sizeof(buf));
    return buf;
}

static const char *cd_long(int32_t s)
{
    ui_format_countdown_long(s, buf, sizeof(buf));
    return buf;
}

static const char *track(float s)
{
    ui_format_track_time(s, buf, sizeof(buf));
    return buf;
}

static void test_format(void)
{
    CHECK_STR(compact(0), "0");
    CHECK_STR(compact(950), "950");
    CHECK_STR(compact(1250), "1.2K");
    CHECK_STR(compact(184000), "184K");
    CHECK_STR(compact(1250000), "1.2M");
    CHECK_STR(compact(12000000), "12.0M");

    CHECK_STR(cd_short(6000), "1:40");
    CHECK_STR(cd_short(3600), "1:00");
    CHECK_STR(cd_short(3599), "60");
    CHECK_STR(cd_short(61), "2"); /* rounds up */
    CHECK_STR(cd_short(1), "1");
    CHECK_STR(cd_short(0), "0");
    CHECK_STR(cd_short(-5), "0");

    CHECK_STR(cd_long(5988), "1:39:48");
    CHECK_STR(cd_long(2388), "39:48");
    CHECK_STR(cd_long(5), "0:05");

    CHECK_STR(track(0), "0:00");
    CHECK_STR(track(102.4f), "1:42");
    CHECK_STR(track(277.1f), "4:37");
    CHECK_STR(track(3725), "1:02:05");
    CHECK_STR(track(-3), "0:00");
}

static void test_json(void)
{
    const char *payload = "{\"title\":\"İncir\",\"n\":42.5,\"b\":true,\"s\":\"x\"}";
    /* Not NUL-terminated at the length we pass, like an MQTT buffer. */
    char raw[128];
    memset(raw, 'Z', sizeof(raw));
    memcpy(raw, payload, strlen(payload));
    cJSON *root = json_parse_payload(raw, (int)strlen(payload));
    CHECK(root != NULL);

    char out[32];
    json_copy_str(root, "title", out, sizeof(out));
    CHECK_STR(out, "İncir");
    json_copy_str(root, "missing", out, sizeof(out));
    CHECK_STR(out, "");
    json_copy_str(root, "n", out, sizeof(out)); /* not a string */
    CHECK_STR(out, "");
    CHECK(json_num(root, "n", 0) == 42.5);
    CHECK(json_num(root, "s", -1) == -1);
    CHECK(json_bool(root, "b", false) == true);
    CHECK(json_bool(root, "n", false) == false);
    cJSON_Delete(root);

    /* Truncation must never leave half a UTF-8 sequence behind. "İ" is 2
     * bytes, "ş" is 2 bytes. */
    root = cJSON_Parse("{\"t\":\"aİşb\"}");
    char small[3]; /* room for 2 bytes: "a" + half of "İ" -> just "a" */
    json_copy_str(root, "t", small, sizeof(small));
    CHECK_STR(small, "a");
    char four[4]; /* 3 bytes: "a" + "İ" fits exactly */
    json_copy_str(root, "t", four, sizeof(four));
    CHECK_STR(four, "aİ");
    char five[5]; /* 4 bytes: "aİ" + half of "ş" -> "aİ" */
    json_copy_str(root, "t", five, sizeof(five));
    CHECK_STR(five, "aİ");
    cJSON_Delete(root);

    root = cJSON_Parse("{\"t\":\"a€b\"}"); /* € is 3 bytes */
    char three[4]; /* "a" + 2 of 3 bytes of € -> "a" */
    json_copy_str(root, "t", three, sizeof(three));
    CHECK_STR(three, "a");
    cJSON_Delete(root);

    CHECK(json_parse_payload("{bad", 4) == NULL);
}

int main(void)
{
    test_format();
    test_json();
    if (s_failures) {
        fprintf(stderr, "%d of %d checks failed\n", s_failures, s_checks);
        return 1;
    }
    printf("All %d checks passed\n", s_checks);
    return 0;
}
