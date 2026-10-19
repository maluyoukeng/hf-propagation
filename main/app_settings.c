#include "app_settings.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "prop_model.h"

#define SETTINGS_MAGIC 0x48504650U
#define SETTINGS_VERSION 1U
#define SETTINGS_NAMESPACE "hfprop"
#define SETTINGS_KEY "config"
#define LOCATION_KEY "location"
#define LOCATION_MAGIC 0x4C4F4350U
#define LOCATION_VERSION 1U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t station;
    uint8_t target;
    int16_t utc_offset_min;
    uint16_t sfi;
    uint8_t kp;
    uint8_t reserved;
    int64_t clock_epoch;
} settings_blob_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t valid;
    uint8_t reserved[3];
    float latitude;
    float longitude;
} location_blob_t;

static nvs_handle_t s_nvs;
static bool s_ready;

static int16_t clamp_i16(int value, int low, int high)
{
    if (value < low) return (int16_t)low;
    if (value > high) return (int16_t)high;
    return (int16_t)value;
}

void app_settings_defaults(app_settings_t *settings)
{
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->station = 0;
    settings->target = 0;
    settings->utc_offset_min = 480;
    settings->sfi = 150;
    settings->kp = 2;
    settings->has_location = false;
}

void app_settings_validate(app_settings_t *settings)
{
    if (!settings) return;
    if (settings->station >= PROP_STATION_COUNT) settings->station = 0;
    if (settings->target >= PROP_TARGET_COUNT) settings->target = 0;
    settings->utc_offset_min = clamp_i16(settings->utc_offset_min, -720, 840);
    if (settings->sfi < 70) settings->sfi = 70;
    if (settings->sfi > 250) settings->sfi = 250;
    if (settings->kp > 9) settings->kp = 9;
    if (settings->clock_epoch < 0) settings->clock_epoch = 0;
    if (settings->has_location &&
        (!isfinite(settings->latitude) || !isfinite(settings->longitude) ||
         settings->latitude < -90.0f || settings->latitude > 90.0f ||
         settings->longitude < -180.0f || settings->longitude > 180.0f)) {
        settings->has_location = false;
        settings->latitude = 0.0f;
        settings->longitude = 0.0f;
    }
}

static void load_location(app_settings_t *settings)
{
    location_blob_t blob;
    size_t length = sizeof(blob);
    if (nvs_get_blob(s_nvs, LOCATION_KEY, &blob, &length) != ESP_OK ||
        length != sizeof(blob) || blob.magic != LOCATION_MAGIC ||
        blob.version != LOCATION_VERSION || blob.size != sizeof(blob) ||
        !blob.valid) {
        return;
    }
    settings->has_location = true;
    settings->latitude = blob.latitude;
    settings->longitude = blob.longitude;
}

esp_err_t app_settings_init(void)
{
    if (s_ready) return ESP_OK;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return err;
    s_ready = true;
    return ESP_OK;
}

esp_err_t app_settings_load(app_settings_t *settings)
{
    if (!settings) return ESP_ERR_INVALID_ARG;
    app_settings_defaults(settings);
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    settings_blob_t blob;
    size_t length = sizeof(blob);
    esp_err_t err = nvs_get_blob(s_nvs, SETTINGS_KEY, &blob, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        load_location(settings);
        app_settings_validate(settings);
        return ESP_OK;
    }
    if (err != ESP_OK) {
        load_location(settings);
        app_settings_validate(settings);
        return err;
    }
    if (length != sizeof(blob) || blob.magic != SETTINGS_MAGIC ||
        blob.version != SETTINGS_VERSION || blob.size != sizeof(blob)) {
        load_location(settings);
        app_settings_validate(settings);
        return ESP_ERR_INVALID_VERSION;
    }

    settings->station = blob.station;
    settings->target = blob.target;
    settings->utc_offset_min = blob.utc_offset_min;
    settings->sfi = blob.sfi;
    settings->kp = blob.kp;
    settings->clock_epoch = blob.clock_epoch;
    load_location(settings);
    app_settings_validate(settings);
    return ESP_OK;
}

esp_err_t app_settings_save(const app_settings_t *settings)
{
    if (!settings) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    const settings_blob_t blob = {
        .magic = SETTINGS_MAGIC,
        .version = SETTINGS_VERSION,
        .size = sizeof(settings_blob_t),
        .station = settings->station,
        .target = settings->target,
        .utc_offset_min = settings->utc_offset_min,
        .sfi = settings->sfi,
        .kp = settings->kp,
        .clock_epoch = settings->clock_epoch,
    };
    esp_err_t err = nvs_set_blob(s_nvs, SETTINGS_KEY, &blob, sizeof(blob));
    if (err == ESP_OK) {
        const location_blob_t location = {
            .magic = LOCATION_MAGIC,
            .version = LOCATION_VERSION,
            .size = sizeof(location_blob_t),
            .valid = settings->has_location ? 1 : 0,
            .latitude = settings->latitude,
            .longitude = settings->longitude,
        };
        err = nvs_set_blob(s_nvs, LOCATION_KEY, &location, sizeof(location));
    }
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    return err;
}

void app_settings_cycle_station(app_settings_t *settings, int direction)
{
    if (!settings || direction == 0) return;
    int value = (int)settings->station + (direction > 0 ? 1 : -1);
    if (value < 0) value = PROP_STATION_COUNT - 1;
    if (value >= PROP_STATION_COUNT) value = 0;
    settings->station = (uint8_t)value;
}

void app_settings_cycle_target(app_settings_t *settings, int direction)
{
    if (!settings || direction == 0) return;
    int value = (int)settings->target + (direction > 0 ? 1 : -1);
    if (value < 0) value = PROP_TARGET_COUNT - 1;
    if (value >= PROP_TARGET_COUNT) value = 0;
    settings->target = (uint8_t)value;
}

void app_settings_adjust_offset(app_settings_t *settings, int delta_minutes)
{
    if (!settings) return;
    settings->utc_offset_min =
        clamp_i16((int)settings->utc_offset_min + delta_minutes, -720, 840);
}

void app_settings_adjust_sfi(app_settings_t *settings, int delta)
{
    if (!settings) return;
    int value = (int)settings->sfi + delta;
    if (value < 70) value = 70;
    if (value > 250) value = 250;
    settings->sfi = (uint16_t)value;
}

void app_settings_adjust_kp(app_settings_t *settings, int delta)
{
    if (!settings) return;
    int value = (int)settings->kp + delta;
    if (value < 0) value = 0;
    if (value > 9) value = 9;
    settings->kp = (uint8_t)value;
}

void app_settings_set_location(app_settings_t *settings, float latitude,
                               float longitude)
{
    if (!settings) return;
    if (!isfinite(latitude) || !isfinite(longitude) ||
        latitude < -90.0f || latitude > 90.0f ||
        longitude < -180.0f || longitude > 180.0f) {
        settings->has_location = false;
        settings->latitude = 0.0f;
        settings->longitude = 0.0f;
        return;
    }
    settings->has_location = true;
    settings->latitude = latitude;
    settings->longitude = longitude;
}

