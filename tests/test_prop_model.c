#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "prop_model.h"

static int failures;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,         \
                    #condition);                                             \
            failures++;                                                      \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                              \
    do {                                                                     \
        const double value = (actual);                                       \
        const double target = (expected);                                    \
        if (fabs(value - target) > (tolerance)) {                            \
            fprintf(stderr, "FAIL %s:%d: %.4f != %.4f\n", __FILE__,         \
                    __LINE__, value, target);                                \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static void test_calendar(void)
{
    CHECK(prop_days_from_civil(1970, 1, 1) == 0);
    CHECK(prop_days_from_civil(2026, 10, 19) == 20745);
    CHECK(prop_day_of_year(2026, 1, 1) == 1);
    CHECK(prop_day_of_year(2024, 3, 1) == 61);
    CHECK(prop_day_of_year(2026, 3, 1) == 60);
    CHECK(prop_day_of_year(2026, 10, 19) == 292);
    CHECK(prop_epoch_from_utc(2000, 1, 1, 12, 0, 0) == 946728000);
    CHECK(prop_epoch_from_utc(2026, 10, 19, 0, 0, 0) == 1792368000);

    prop_utc_t time;
    prop_utc_from_epoch(0, &time);
    CHECK(time.year == 1970 && time.month == 1 && time.day == 1);
    CHECK(time.hour == 0 && time.minute == 0 && time.second == 0);

    prop_utc_from_epoch(1792368000, &time);
    CHECK(time.year == 2026 && time.month == 10 && time.day == 19);
    CHECK(time.day_of_year == 292);

    int year;
    unsigned month;
    unsigned day;
    prop_civil_from_days(20745, &year, &month, &day);
    CHECK(year == 2026 && month == 10 && day == 19);
}

static void test_solar_geometry(void)
{
    CHECK_NEAR(prop_solar_declination_deg(80), 0.0, 1.5);

    const float noon = prop_solar_zenith_deg(0.0f, 0.0f, 12.0f, 80);
    CHECK_NEAR(noon, 0.0, 4.0);

    const float midnight = prop_solar_zenith_deg(0.0f, 0.0f, 0.0f, 80);
    CHECK(midnight > 120.0f);

    float distance = 0.0f;
    float bearing = 0.0f;
    float mid_lat = 0.0f;
    float mid_lon = 0.0f;
    prop_great_circle(39.90f, 116.41f, 51.51f, -0.13f,
                      &distance, &bearing, &mid_lat, &mid_lon);
    CHECK_NEAR(distance, 8141.0, 60.0);
    CHECK_NEAR(bearing, 324.4, 2.0);
    CHECK(mid_lat > 58.0f && mid_lat < 68.0f);
    CHECK(mid_lon > 55.0f && mid_lon < 80.0f);

    prop_great_circle(39.90f, 116.41f, 34.05f, -118.24f,
                      &distance, &bearing, &mid_lat, &mid_lon);
    CHECK_NEAR(distance, 10062.0, 80.0);
    CHECK(mid_lat > 55.0f && mid_lat < 62.0f);
    CHECK(mid_lon < -170.0f || mid_lon > 170.0f);
}

static prop_input_t base_input(void)
{
    prop_input_t input = {
        .station = 0,
        .target = 2,
        .utc_offset_min = 480,
        .sfi = 150,
        .kp = 2,
    };
    return input;
}

static void test_forecast(void)
{
    const int64_t epoch = prop_epoch_from_utc(2026, 10, 19, 4, 0, 0);
    prop_input_t input = base_input();
    prop_forecast_t forecast;

    CHECK(prop_forecast(&input, epoch, &forecast));
    CHECK(forecast.conditions.muf_mhz > 8.0f);
    CHECK(forecast.conditions.muf_mhz < 32.0f);
    CHECK(forecast.conditions.luf_mhz < forecast.conditions.muf_mhz);
    CHECK(forecast.conditions.distance_km > 7000.0f);
    CHECK(forecast.best_band >= 0 && forecast.best_band < PROP_BAND_COUNT);
    CHECK(forecast.overall_score >= 0 && forecast.overall_score <= 100);
    CHECK(forecast.overall_score == forecast.bands[forecast.best_band].score);
    CHECK(forecast.overall_rating == forecast.bands[forecast.best_band].rating);

    for (int i = 0; i < PROP_BAND_COUNT; i++) {
        CHECK(forecast.bands[i].score >= 0 && forecast.bands[i].score <= 100);
        CHECK(forecast.bands[i].frequency_mhz > 0.0f);
        CHECK(forecast.bands[i].rating >= PROP_RATING_POOR);
        CHECK(forecast.bands[i].rating <= PROP_RATING_EXCELLENT);
    }

    prop_input_t quiet = input;
    quiet.sfi = 90;
    prop_forecast_t quiet_forecast;
    CHECK(prop_forecast(&quiet, epoch, &quiet_forecast));
    CHECK(forecast.conditions.muf_mhz >= quiet_forecast.conditions.muf_mhz);
    CHECK(forecast.overall_score >= quiet_forecast.overall_score);

    prop_input_t active = input;
    active.sfi = 220;
    prop_forecast_t active_forecast;
    CHECK(prop_forecast(&active, epoch, &active_forecast));
    CHECK(active_forecast.conditions.muf_mhz >= forecast.conditions.muf_mhz);

    prop_input_t storm = input;
    storm.kp = 8;
    prop_forecast_t storm_forecast;
    CHECK(prop_forecast(&storm, epoch, &storm_forecast));
    CHECK(storm_forecast.conditions.muf_mhz <= forecast.conditions.muf_mhz);
    CHECK(storm_forecast.overall_score <= forecast.overall_score);

    prop_window_t window;
    CHECK(prop_best_window(&input, epoch, forecast.best_band, &window));
    CHECK(window.start_minute >= 0 && window.start_minute < 1440);
    CHECK(window.end_minute > window.start_minute);
    CHECK(window.end_minute <= 1440);
    CHECK(window.peak_score >= 0 && window.peak_score <= 100);
}

static void test_auto_location(void)
{
    const int64_t epoch = prop_epoch_from_utc(2026, 10, 19, 4, 0, 0);
    prop_input_t beijing = base_input();
    beijing.station = 0;
    prop_forecast_t beijing_forecast;
    CHECK(prop_forecast(&beijing, epoch, &beijing_forecast));

    prop_input_t auto_beijing = beijing;
    auto_beijing.station = PROP_STATION_AUTO;
    auto_beijing.has_location = true;
    auto_beijing.latitude = PROP_STATIONS[0].lat;
    auto_beijing.longitude = PROP_STATIONS[0].lon;
    prop_forecast_t auto_forecast;
    CHECK(prop_forecast(&auto_beijing, epoch, &auto_forecast));
    CHECK_NEAR(auto_forecast.conditions.distance_km,
               beijing_forecast.conditions.distance_km, 0.01);
    CHECK_NEAR(auto_forecast.overall_score, beijing_forecast.overall_score, 1);

    prop_input_t shanghai = beijing;
    shanghai.station = 1;
    prop_forecast_t shanghai_forecast;
    CHECK(prop_forecast(&shanghai, epoch, &shanghai_forecast));

    prop_input_t auto_shanghai = shanghai;
    auto_shanghai.station = PROP_STATION_AUTO;
    auto_shanghai.has_location = true;
    auto_shanghai.latitude = PROP_STATIONS[1].lat;
    auto_shanghai.longitude = PROP_STATIONS[1].lon;
    CHECK(prop_forecast(&auto_shanghai, epoch, &auto_forecast));
    CHECK_NEAR(auto_forecast.conditions.distance_km,
               shanghai_forecast.conditions.distance_km, 0.01);

    prop_input_t auto_pending = beijing;
    auto_pending.station = PROP_STATION_AUTO;
    auto_pending.has_location = false;
    prop_forecast_t pending_forecast;
    CHECK(prop_forecast(&auto_pending, epoch, &pending_forecast));
    CHECK_NEAR(pending_forecast.conditions.distance_km,
               beijing_forecast.conditions.distance_km, 0.01);
}

static void test_day_night_absorption(void)
{
    prop_input_t input = base_input();
    prop_forecast_t day;
    prop_forecast_t night;
    CHECK(prop_forecast(&input, prop_epoch_from_utc(2026, 6, 21, 4, 0, 0), &day));
    CHECK(prop_forecast(&input, prop_epoch_from_utc(2026, 6, 21, 16, 0, 0), &night));
    CHECK(day.conditions.luf_mhz > night.conditions.luf_mhz);
    CHECK(day.conditions.daylight_fraction > night.conditions.daylight_fraction);
}

int main(void)
{
    test_calendar();
    test_solar_geometry();
    test_forecast();
    test_auto_location();
    test_day_night_absorption();
    if (failures != 0) {
        fprintf(stderr, "%d checks failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("prop_model tests: PASS\n");
    return EXIT_SUCCESS;
}

