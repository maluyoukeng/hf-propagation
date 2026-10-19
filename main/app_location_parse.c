#include "app_location_parse.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_value(const char *json, const char *key)
{
    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *cursor = strstr(json, pattern);
    if (!cursor) return NULL;
    cursor += strlen(pattern);
    while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    if (*cursor != ':') return NULL;
    cursor++;
    while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    return cursor;
}

static bool json_number(const char *json, const char *key, float *out)
{
    const char *cursor = find_value(json, key);
    if (!cursor) return false;
    char *end = NULL;
    const float value = strtof(cursor, &end);
    if (end == cursor || !isfinite(value)) return false;
    *out = value;
    return true;
}

static bool json_string(const char *json, const char *key, char *out,
                        size_t out_size)
{
    const char *cursor = find_value(json, key);
    if (!cursor || *cursor != '"' || out_size == 0) return false;
    cursor++;
    size_t used = 0;
    while (*cursor && *cursor != '"') {
        char value = *cursor++;
        if (value == '\\' && *cursor) {
            value = *cursor++;
            if (value == 'n') value = ' ';
        }
        if (value == '\r' || value == '\n') value = ' ';
        if (used + 1 < out_size) out[used++] = value;
    }
    out[used] = '\0';
    return used > 0;
}

static bool json_bool(const char *json, const char *key, bool *out)
{
    const char *cursor = find_value(json, key);
    if (!cursor) return false;
    if (strncmp(cursor, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(cursor, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}

bool app_location_parse(const char *json, app_location_t *out)
{
    if (!json || !out) return false;
    memset(out, 0, sizeof(*out));

    bool flag = false;
    if (json_bool(json, "success", &flag) && !flag) return false;
    if (json_bool(json, "error", &flag) && flag) return false;

    char status[16];
    if (json_string(json, "status", status, sizeof(status)) &&
        strcmp(status, "success") != 0) {
        return false;
    }

    float latitude = 0.0f;
    float longitude = 0.0f;
    if (!json_number(json, "latitude", &latitude)) {
        if (!json_number(json, "lat", &latitude)) return false;
    }
    if (!json_number(json, "longitude", &longitude)) {
        if (!json_number(json, "lon", &longitude) &&
            !json_number(json, "lng", &longitude)) {
            return false;
        }
    }
    if (latitude < -90.0f || latitude > 90.0f ||
        longitude < -180.0f || longitude > 180.0f ||
        (fabsf(latitude) < 0.001f && fabsf(longitude) < 0.001f)) {
        return false;
    }

    if (!json_string(json, "city", out->city, sizeof(out->city))) {
        (void)json_string(json, "cityName", out->city, sizeof(out->city));
    }
    if (!json_string(json, "region", out->region, sizeof(out->region))) {
        (void)json_string(json, "regionName", out->region,
                          sizeof(out->region));
    }
    out->latitude = latitude;
    out->longitude = longitude;
    out->valid = true;
    return true;
}
