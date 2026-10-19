#pragma once

#include <stdbool.h>

typedef struct {
    bool valid;
    float latitude;
    float longitude;
    char city[32];
    char region[32];
} app_location_t;

bool app_location_parse(const char *json, app_location_t *out);
