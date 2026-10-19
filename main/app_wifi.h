#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "app_location.h"
#include "esp_err.h"

typedef enum {
    APP_WIFI_IDLE = 0,
    APP_WIFI_PORTAL_STARTING,
    APP_WIFI_PORTAL_READY,
    APP_WIFI_CONFIG_SAVED,
    APP_WIFI_CONNECTING,
    APP_WIFI_LOCATING,
    APP_WIFI_FETCHING,
    APP_WIFI_SUCCESS,
    APP_WIFI_FAILED,
} app_wifi_state_t;

typedef struct {
    app_wifi_state_t state;
    char ap_ssid[33];
    char ap_password[16];
    char router_ssid[33];
    bool has_config;
    int sfi;
    int kp;
    int a_index;
    char issued[32];
    char message[96];
} app_wifi_status_t;

esp_err_t app_wifi_init(void);
esp_err_t app_wifi_start_portal(void);
esp_err_t app_wifi_start_update(bool fetch_location);
esp_err_t app_wifi_request_stop(void);
void app_wifi_status(app_wifi_status_t *out);
bool app_wifi_take_result(int *sfi, int *kp, int *a_index,
                          char *issued, size_t issued_size);
bool app_wifi_take_location(app_location_t *out);

