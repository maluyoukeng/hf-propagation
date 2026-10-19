#pragma once

#include <stdbool.h>

typedef struct {
    int sfi;
    int kp;
    int a_index;
    char issued[32];
} space_weather_t;

bool space_weather_parse_wwv(const char *text, space_weather_t *out);

