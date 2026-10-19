#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PROP_BAND_COUNT 8
#define PROP_STATION_AUTO 8
#define PROP_STATION_COUNT 9
#define PROP_TARGET_COUNT 6

typedef enum {
    PROP_RATING_POOR = 0,
    PROP_RATING_FAIR,
    PROP_RATING_GOOD,
    PROP_RATING_EXCELLENT,
} prop_rating_t;

typedef struct {
    const char *name;
    float lat;
    float lon;
} prop_place_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t day_of_year;
} prop_utc_t;

typedef struct {
    uint8_t station;
    uint8_t target;
    int16_t utc_offset_min;
    uint16_t sfi;
    uint8_t kp;
    bool has_location;
    float latitude;
    float longitude;
} prop_input_t;

typedef struct {
    float muf_mhz;
    float luf_mhz;
    float fof2_mhz;
    float distance_km;
    float bearing_deg;
    float daylight_fraction;
    float zenith_deg;
    float cos_zenith;
} prop_conditions_t;

typedef struct {
    float frequency_mhz;
    int score;
    prop_rating_t rating;
} prop_band_t;

typedef struct {
    prop_conditions_t conditions;
    prop_band_t bands[PROP_BAND_COUNT];
    int best_band;
    int overall_score;
    prop_rating_t overall_rating;
} prop_forecast_t;

typedef struct {
    int start_minute;
    int end_minute;
    int peak_score;
} prop_window_t;

extern const prop_place_t PROP_STATIONS[PROP_STATION_COUNT];
extern const prop_place_t PROP_TARGETS[PROP_TARGET_COUNT];

const char *prop_band_name(int index);
float prop_band_frequency_mhz(int index);
const char *prop_rating_text(prop_rating_t rating);
uint32_t prop_rating_color(prop_rating_t rating);

int64_t prop_days_from_civil(int year, unsigned month, unsigned day);
void prop_civil_from_days(int64_t days, int *year, unsigned *month, unsigned *day);
uint32_t prop_day_of_year(int year, unsigned month, unsigned day);
void prop_utc_from_epoch(int64_t epoch, prop_utc_t *out);
int64_t prop_epoch_from_utc(int year, unsigned month, unsigned day,
                            unsigned hour, unsigned minute, unsigned second);

float prop_solar_declination_deg(uint32_t day_of_year);
float prop_solar_zenith_deg(float lat, float lon, float utc_hours,
                            uint32_t day_of_year);
void prop_great_circle(float lat1, float lon1, float lat2, float lon2,
                       float *distance_km, float *bearing_deg,
                       float *mid_lat, float *mid_lon);

bool prop_forecast(const prop_input_t *input, int64_t epoch,
                   prop_forecast_t *out);
bool prop_best_window(const prop_input_t *input, int64_t epoch, int band_index,
                      prop_window_t *out);

