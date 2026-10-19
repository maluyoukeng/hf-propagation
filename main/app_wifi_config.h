#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
    char ssid[33];
    char password[65];
} app_wifi_config_t;

esp_err_t app_wifi_config_init(void);
esp_err_t app_wifi_config_load(app_wifi_config_t *out);
esp_err_t app_wifi_config_save(const app_wifi_config_t *config);
bool app_wifi_config_valid(const app_wifi_config_t *config);

