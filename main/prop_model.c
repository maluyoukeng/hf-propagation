#include "prop_model.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define PROP_PI 3.14159265358979323846f
#define PROP_EARTH_RADIUS_KM 6371.0f

const prop_place_t PROP_STATIONS[PROP_STATION_COUNT] = {
    {"北京", 39.90f, 116.41f},
    {"上海", 31.23f, 121.47f},
    {"广州", 23.13f, 113.26f},
    {"成都", 30.57f, 104.07f},
    {"乌鲁木齐", 43.83f, 87.62f},
    {"哈尔滨", 45.80f, 126.53f},
    {"拉萨", 29.65f, 91.14f},
    {"海口", 20.04f, 110.20f},
    {"自动定位", 39.90f, 116.41f},
};

const prop_place_t PROP_TARGETS[PROP_TARGET_COUNT] = {
    {"东亚", 35.68f, 139.69f},
    {"东南亚", 1.35f, 103.82f},
    {"欧洲", 51.51f, -0.13f},
    {"北美", 34.05f, -118.24f},
    {"大洋洲", -33.87f, 151.21f},
    {"南美", -23.55f, -46.63f},
};

static const char *const BAND_NAMES[PROP_BAND_COUNT] = {
    "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m",
};

static const float BAND_FREQ_MHZ[PROP_BAND_COUNT] = {
    3.60f, 7.10f, 10.12f, 14.10f, 18.10f, 21.10f, 24.92f, 28.50f,
};

static float clampf(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static float deg2rad(float deg)
{
    return deg * PROP_PI / 180.0f;
}

static float rad2deg(float rad)
{
    return rad * 180.0f / PROP_PI;
}

const char *prop_band_name(int index)
{
    if (index < 0 || index >= PROP_BAND_COUNT) return "--";
    return BAND_NAMES[index];
}

float prop_band_frequency_mhz(int index)
{
    if (index < 0 || index >= PROP_BAND_COUNT) return 0.0f;
    return BAND_FREQ_MHZ[index];
}

const char *prop_rating_text(prop_rating_t rating)
{
    switch (rating) {
    case PROP_RATING_EXCELLENT: return "优";
    case PROP_RATING_GOOD: return "良";
    case PROP_RATING_FAIR: return "一般";
    default: return "差";
    }
}

uint32_t prop_rating_color(prop_rating_t rating)
{
    switch (rating) {
    case PROP_RATING_EXCELLENT: return 0x35D07F;
    case PROP_RATING_GOOD: return 0x8BD450;
    case PROP_RATING_FAIR: return 0xF4B942;
    default: return 0xE45B4E;
    }
}

int64_t prop_days_from_civil(int year, unsigned month, unsigned day)
{
    const int y = year - (month <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 +
                         day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

void prop_civil_from_days(int64_t days, int *year, unsigned *month, unsigned *day)
{
    const int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = (int)yoe + (int)era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;

    if (year) *year = y + (m <= 2);
    if (month) *month = m;
    if (day) *day = d;
}

uint32_t prop_day_of_year(int year, unsigned month, unsigned day)
{
    return (uint32_t)(prop_days_from_civil(year, month, day) -
                      prop_days_from_civil(year, 1, 1) + 1);
}

void prop_utc_from_epoch(int64_t epoch, prop_utc_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));

    int64_t days = epoch / 86400;
    int64_t seconds = epoch % 86400;
    if (seconds < 0) {
        seconds += 86400;
        days -= 1;
    }

    int year;
    unsigned month;
    unsigned day;
    prop_civil_from_days(days, &year, &month, &day);

    out->year = (uint16_t)year;
    out->month = (uint8_t)month;
    out->day = (uint8_t)day;
    out->hour = (uint8_t)(seconds / 3600);
    out->minute = (uint8_t)((seconds % 3600) / 60);
    out->second = (uint8_t)(seconds % 60);
    out->day_of_year = (uint16_t)prop_day_of_year(year, month, day);
}

int64_t prop_epoch_from_utc(int year, unsigned month, unsigned day,
                            unsigned hour, unsigned minute, unsigned second)
{
    return prop_days_from_civil(year, month, day) * 86400 +
           (int64_t)hour * 3600 + (int64_t)minute * 60 + (int64_t)second;
}

float prop_solar_declination_deg(uint32_t day_of_year)
{
    return 23.44f * sinf(2.0f * PROP_PI * (284.0f + (float)day_of_year) / 365.0f);
}

static float solar_hour_angle_deg(float lon, float utc_hours, uint32_t day_of_year)
{
    const float b = 2.0f * PROP_PI * ((float)day_of_year - 81.0f) / 364.0f;
    const float equation_of_time_min =
        9.87f * sinf(2.0f * b) - 7.53f * cosf(b) - 1.5f * sinf(b);
    const float solar_hours = utc_hours + lon / 15.0f + equation_of_time_min / 60.0f;
    return 15.0f * (solar_hours - 12.0f);
}

float prop_solar_zenith_deg(float lat, float lon, float utc_hours,
                            uint32_t day_of_year)
{
    const float decl = deg2rad(prop_solar_declination_deg(day_of_year));
    const float hour_angle = deg2rad(
        solar_hour_angle_deg(lon, utc_hours, day_of_year));
    const float lat_rad = deg2rad(lat);
    const float cos_z = sinf(lat_rad) * sinf(decl) +
                        cosf(lat_rad) * cosf(decl) * cosf(hour_angle);
    return rad2deg(acosf(clampf(cos_z, -1.0f, 1.0f)));
}

void prop_great_circle(float lat1, float lon1, float lat2, float lon2,
                       float *distance_km, float *bearing_deg,
                       float *mid_lat, float *mid_lon)
{
    const float lat1r = deg2rad(lat1);
    const float lat2r = deg2rad(lat2);
    const float dlat = lat2r - lat1r;
    const float dlon = deg2rad(lon2 - lon1);
    const float a = sinf(dlat / 2.0f) * sinf(dlat / 2.0f) +
                    cosf(lat1r) * cosf(lat2r) *
                        sinf(dlon / 2.0f) * sinf(dlon / 2.0f);
    const float d = 2.0f * atan2f(sqrtf(a), sqrtf(clampf(1.0f - a, 0.0f, 1.0f)));
    const float bearing = atan2f(
        sinf(dlon) * cosf(lat2r),
        cosf(lat1r) * sinf(lat2r) - sinf(lat1r) * cosf(lat2r) * cosf(dlon));

    if (distance_km) *distance_km = d * PROP_EARTH_RADIUS_KM;
    if (bearing_deg) *bearing_deg = fmodf(rad2deg(bearing) + 360.0f, 360.0f);

    const float bx = cosf(lat2r) * cosf(dlon);
    const float by = cosf(lat2r) * sinf(dlon);
    const float computed_mid_lat = rad2deg(atan2f(
        sinf(lat1r) + sinf(lat2r),
        sqrtf((cosf(lat1r) + bx) * (cosf(lat1r) + bx) + by * by)));
    if (mid_lat) *mid_lat = computed_mid_lat;
    if (mid_lon) {
        *mid_lon = lon1 + rad2deg(atan2f(by, cosf(lat1r) + bx));
        if (*mid_lon > 180.0f) *mid_lon -= 360.0f;
        if (*mid_lon < -180.0f) *mid_lon += 360.0f;
    }
}

static void point_along_path(float lat1, float lon1, float bearing_rad,
                             float angular_distance, float fraction,
                             float *lat_out, float *lon_out)
{
    const float lat1r = deg2rad(lat1);
    const float distance = angular_distance * fraction;
    const float lat = asinf(clampf(
        sinf(lat1r) * cosf(distance) +
            cosf(lat1r) * sinf(distance) * cosf(bearing_rad),
        -1.0f, 1.0f));
    const float lon = deg2rad(lon1) +
        atan2f(sinf(bearing_rad) * sinf(distance) * cosf(lat1r),
               cosf(distance) - sinf(lat1r) * sinf(lat));

    if (lat_out) *lat_out = rad2deg(lat);
    if (lon_out) *lon_out = fmodf(rad2deg(lon) + 540.0f, 360.0f) - 180.0f;
}

static prop_rating_t rating_from_score(int score)
{
    if (score >= 75) return PROP_RATING_EXCELLENT;
    if (score >= 55) return PROP_RATING_GOOD;
    if (score >= 35) return PROP_RATING_FAIR;
    return PROP_RATING_POOR;
}

static int score_band(float frequency, float muf, float luf, int kp,
                      float daylight_fraction)
{
    float score;

    if (frequency > muf) {
        score = 52.0f * expf(-(frequency - muf) / 2.2f) - 6.0f;
    } else if (frequency < luf) {
        score = 48.0f * expf(-(luf - frequency) / 2.6f);
    } else {
        const float above = frequency - luf;
        const float below = muf - frequency;
        const float edge = (1.0f - expf(-above / 2.2f)) *
                           (1.0f - expf(-below / 2.2f));
        score = 40.0f + 60.0f * edge;
    }

    if (kp > 3) {
        const float high_band = frequency >= 14.0f ? 1.35f : 0.75f;
        score -= (float)(kp - 3) * 3.2f * high_band;
    }

    score *= 0.90f + 0.10f * clampf(daylight_fraction, 0.0f, 1.0f);
    score = clampf(score, 0.0f, 100.0f);
    return (int)(score + 0.5f);
}

static bool conditions_for(const prop_input_t *input, int64_t epoch,
                           prop_conditions_t *conditions)
{
    if (!input || !conditions) return false;
    if (input->station >= PROP_STATION_COUNT || input->target >= PROP_TARGET_COUNT) {
        return false;
    }

    prop_utc_t utc_time;
    prop_utc_from_epoch(epoch, &utc_time);
    const float utc_hours = (float)utc_time.hour + (float)utc_time.minute / 60.0f;
    const prop_place_t *station = &PROP_STATIONS[input->station];
    prop_place_t auto_place;
    if (input->station == PROP_STATION_AUTO && input->has_location &&
        isfinite(input->latitude) && isfinite(input->longitude) &&
        input->latitude >= -90.0f && input->latitude <= 90.0f &&
        input->longitude >= -180.0f && input->longitude <= 180.0f) {
        auto_place = *station;
        auto_place.lat = input->latitude;
        auto_place.lon = input->longitude;
        station = &auto_place;
    }
    const prop_place_t *target = &PROP_TARGETS[input->target];

    float distance = 0.0f;
    float bearing = 0.0f;
    float mid_lat = 0.0f;
    float mid_lon = 0.0f;
    prop_great_circle(station->lat, station->lon, target->lat, target->lon,
                      &distance, &bearing, &mid_lat, &mid_lon);

    float cos_sum = 0.0f;
    int daylight_points = 0;
    const int samples = 5;
    const float bearing_rad = deg2rad(bearing);
    const float angular_distance = distance / PROP_EARTH_RADIUS_KM;
    for (int i = 0; i < samples; i++) {
        const float t = (float)i / (float)(samples - 1);
        float lat = station->lat;
        float lon = station->lon;
        point_along_path(station->lat, station->lon, bearing_rad,
                         angular_distance, t, &lat, &lon);
        const float zenith = prop_solar_zenith_deg(lat, lon, utc_hours,
                                                   utc_time.day_of_year);
        cos_sum += fmaxf(0.0f, cosf(deg2rad(zenith)));
        if (zenith < 90.0f) daylight_points++;
    }

    const float zenith_mid = prop_solar_zenith_deg(mid_lat, mid_lon, utc_hours,
                                                   utc_time.day_of_year);
    const float cos_mid = fmaxf(0.0f, cosf(deg2rad(zenith_mid)));
    const float cos_avg = cos_sum / (float)samples;
    const float daylight_fraction = (float)daylight_points / (float)samples;

    const float sfi = (float)clampf((float)input->sfi, 70.0f, 300.0f);
    const float kp = (float)(input->kp > 9 ? 9 : input->kp);
    const float ssnumber = fmaxf(0.0f, (sfi - 67.0f) / 0.97f);

    const float seasonal =
        0.45f * sinf(2.0f * PROP_PI * ((float)utc_time.day_of_year - 80.0f) / 365.0f);
    const float day_weight = clampf(cos_avg * 1.5f, 0.0f, 1.0f);
    const float fof2_day = 3.8f + 0.028f * ssnumber +
                           1.60f * powf(cos_avg, 1.1f) + seasonal;
    const float fof2_night = 2.2f + 0.018f * ssnumber;
    float fof2 = fof2_night + (fof2_day - fof2_night) * day_weight;

    const float storm = clampf((kp - 2.0f) / 7.0f, 0.0f, 1.0f);
    const float latitude_factor =
        0.45f + 0.55f * fabsf(sinf(deg2rad(mid_lat)));
    fof2 *= 1.0f - 0.30f * storm * latitude_factor;

    const float m_factor = 2.60f + 0.50f * fabsf(cosf(deg2rad(mid_lat)));
    const float muf = fmaxf(2.0f, fof2 * m_factor);
    const float luf = 1.7f + 3.8f * sqrtf(cos_mid) +
                      0.005f * fmaxf(0.0f, sfi - 70.0f) +
                      0.8f * storm;

    conditions->muf_mhz = muf;
    conditions->luf_mhz = fminf(luf, muf - 0.4f);
    conditions->fof2_mhz = fof2;
    conditions->distance_km = distance;
    conditions->bearing_deg = bearing;
    conditions->daylight_fraction = daylight_fraction;
    conditions->zenith_deg = zenith_mid;
    conditions->cos_zenith = cos_mid;
    return true;
}

bool prop_forecast(const prop_input_t *input, int64_t epoch,
                   prop_forecast_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!conditions_for(input, epoch, &out->conditions)) return false;

    int best_score = -1;
    for (int i = 0; i < PROP_BAND_COUNT; i++) {
        const int score = score_band(BAND_FREQ_MHZ[i], out->conditions.muf_mhz,
                                     out->conditions.luf_mhz,
                                     input->kp > 9 ? 9 : input->kp,
                                     out->conditions.daylight_fraction);
        out->bands[i].frequency_mhz = BAND_FREQ_MHZ[i];
        out->bands[i].score = score;
        out->bands[i].rating = rating_from_score(score);
        if (score > best_score) {
            best_score = score;
            out->best_band = i;
        }
    }

    if (best_score < 0) best_score = 0;
    out->overall_score = best_score;
    out->overall_rating = rating_from_score(best_score);
    return true;
}

bool prop_best_window(const prop_input_t *input, int64_t epoch, int band_index,
                      prop_window_t *out)
{
    if (!out || band_index < 0 || band_index >= PROP_BAND_COUNT) return false;
    memset(out, 0, sizeof(*out));

    const int64_t day_start = epoch - (epoch % 86400);
    int scores[48];
    int peak = 0;
    int peak_score = -1;
    for (int slot = 0; slot < 48; slot++) {
        prop_forecast_t forecast;
        if (!prop_forecast(input, day_start + (int64_t)slot * 1800, &forecast)) {
            return false;
        }
        scores[slot] = forecast.bands[band_index].score;
        if (scores[slot] > peak_score) {
            peak_score = scores[slot];
            peak = slot;
        }
    }

    int first = peak;
    int last = peak;
    while (first > 0 && scores[first - 1] >= 55) first--;
    while (last < 47 && scores[last + 1] >= 55) last++;

    if (first == last && peak_score < 55) {
        first = peak;
        last = peak;
    }

    out->start_minute = first * 30;
    out->end_minute = (last + 1) * 30;
    out->peak_score = peak_score;
    return true;
}

