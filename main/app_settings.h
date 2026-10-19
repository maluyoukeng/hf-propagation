#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    APP_SETTING_STATION = 0,
    APP_SETTING_TARGET,
    APP_SETTING_UTC_OFFSET,
    APP_SETTING_SFI,
    APP_SETTING_KP,
    APP_SETTING_TIME,
    APP_SETTING_DATE,
    APP_SETTING_COUNT,
} app_setting_field_t;

typedef struct {
    uint8_t station;
    uint8_t target;
    int16_t utc_offset_min;
    uint16_t sfi;
    uint8_t kp;
    int64_t clock_epoch;
    bool has_location;
    float latitude;
    float longitude;
} app_settings_t;

void app_settings_defaults(app_settings_t *settings);
void app_settings_validate(app_settings_t *settings);

esp_err_t app_settings_init(void);
esp_err_t app_settings_load(app_settings_t *settings);
esp_err_t app_settings_save(const app_settings_t *settings);

void app_settings_cycle_station(app_settings_t *settings, int direction);
void app_settings_cycle_target(app_settings_t *settings, int direction);
void app_settings_adjust_offset(app_settings_t *settings, int delta_minutes);
void app_settings_adjust_sfi(app_settings_t *settings, int delta);
void app_settings_adjust_kp(app_settings_t *settings, int delta);
void app_settings_set_location(app_settings_t *settings, float latitude,
                               float longitude);

