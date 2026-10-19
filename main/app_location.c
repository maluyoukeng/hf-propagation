#include "app_location.h"

#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define TAG "app_location"
#define LOCATION_BUFFER_SIZE 2048
#define LOCATION_TIMEOUT_MS 5000

static char s_response[LOCATION_BUFFER_SIZE];
static size_t s_response_len;

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_DATA || !event->data) {
        return ESP_OK;
    }
    size_t available = sizeof(s_response) - 1 - s_response_len;
    size_t copy = (size_t)event->data_len;
    if (copy > available) copy = available;
    if (copy) {
        memcpy(s_response + s_response_len, event->data, copy);
        s_response_len += copy;
        s_response[s_response_len] = '\0';
    }
    return ESP_OK;
}

static esp_err_t fetch_json_once(const char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .timeout_ms = LOCATION_TIMEOUT_MS,
        .buffer_size = 512,
        .buffer_size_tx = 256,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;
    (void)esp_http_client_set_header(client, "User-Agent", "AI-Passport/1.0");
    s_response_len = 0;
    s_response[0] = '\0';

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) return err;
    if (status != 200) {
        ESP_LOGW(TAG, "HTTP status %d from %s", status, url);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t app_location_fetch(app_location_t *out)
{
    static const char *const URLS[] = {
        "http://ip-api.com/json/?fields=status,country,regionName,city,lat,lon",
        "https://ipapi.co/json/",
        "https://ipwho.is/",
        "https://freeipapi.com/api/json",
    };

    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < sizeof(URLS) / sizeof(URLS[0]); i++) {
        esp_err_t err = fetch_json_once(URLS[i]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "location fetch failed: %s", esp_err_to_name(err));
            continue;
        }
        if (app_location_parse(s_response, out)) {
            ESP_LOGI(TAG, "location from %s: %.4f,%.4f city=%s region=%s",
                     URLS[i], (double)out->latitude, (double)out->longitude,
                     out->city, out->region);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "location parse failed: %.120s", s_response);
    }
    return ESP_FAIL;
}
