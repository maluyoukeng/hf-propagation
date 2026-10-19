#include "app_spaceweather.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_int_after(const char *text, const char *marker, int *out)
{
    if (!text || !marker || !out) return false;
    const char *cursor = strstr(text, marker);
    if (!cursor) return false;
    cursor += strlen(marker);
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    char *end = NULL;
    long value = strtol(cursor, &end, 10);
    if (end == cursor) return false;
    *out = (int)value;
    return true;
}

static void parse_issued(const char *text, char *out, size_t out_size)
{
    if (!text || !out || out_size == 0) return;
    out[0] = '\0';
    const char *cursor = strstr(text, ":Issued:");
    if (!cursor) return;
    cursor += strlen(":Issued:");
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    size_t used = 0;
    while (*cursor && *cursor != '\r' && *cursor != '\n' && used + 1 < out_size) {
        out[used++] = *cursor++;
    }
    while (used > 0 && (out[used - 1] == ' ' || out[used - 1] == '\t')) used--;
    out[used] = '\0';
}

bool space_weather_parse_wwv(const char *text, space_weather_t *out)
{
    if (!text || !out) return false;
    memset(out, 0, sizeof(*out));

    int sfi = 0;
    int a_index = -1;
    if (!parse_int_after(text, "Solar flux ", &sfi)) return false;
    if (sfi < 50 || sfi > 300) return false;
    (void)parse_int_after(text, "planetary A-index ", &a_index);
    if (a_index < 0 || a_index > 400) a_index = -1;

    int kp = 0;
    const char *kp_marker = strstr(text, "planetary K-index");
    if (kp_marker) {
        const char *was = strstr(kp_marker, "was ");
        if (was) {
            char *end = NULL;
            float value = strtof(was + 4, &end);
            if (end != was + 4) {
                kp = (int)floorf(value + 0.5f);
            }
        }
    }
    if (kp < 0) kp = 0;
    if (kp > 9) kp = 9;

    out->sfi = sfi;
    out->kp = kp;
    out->a_index = a_index;
    parse_issued(text, out->issued, sizeof(out->issued));
    return true;
}

