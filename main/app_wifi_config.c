#include "app_wifi_config.h"

#include <stddef.h>
#include <string.h>

#include "nvs.h"

#define WIFI_NAMESPACE "hfprop"
#define WIFI_KEY "wifi"

static nvs_handle_t s_nvs;
static bool s_ready;

esp_err_t app_wifi_config_init(void)
{
    if (s_ready) return ESP_OK;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return err;
    s_ready = true;
    return ESP_OK;
}

bool app_wifi_config_valid(const app_wifi_config_t *config)
{
    if (!config) return false;
    size_t ssid_len = strnlen(config->ssid, sizeof(config->ssid));
    size_t password_len = strnlen(config->password, sizeof(config->password));
    return ssid_len > 0 && ssid_len < sizeof(config->ssid) &&
           password_len < sizeof(config->password);
}

esp_err_t app_wifi_config_load(app_wifi_config_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    size_t length = sizeof(*out);
    esp_err_t err = nvs_get_blob(s_nvs, WIFI_KEY, out, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    if (length != sizeof(*out) || !app_wifi_config_valid(out)) {
        memset(out, 0, sizeof(*out));
        return ESP_ERR_INVALID_SIZE;
    }
    out->ssid[sizeof(out->ssid) - 1] = '\0';
    out->password[sizeof(out->password) - 1] = '\0';
    return ESP_OK;
}

esp_err_t app_wifi_config_save(const app_wifi_config_t *config)
{
    if (!config || !app_wifi_config_valid(config)) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_blob(s_nvs, WIFI_KEY, config, sizeof(*config));
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    return err;
}

