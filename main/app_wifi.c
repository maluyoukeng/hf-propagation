#include "app_wifi.h"

#include <stdio.h>
#include <string.h>

#include "app_spaceweather.h"
#include "app_wifi_config.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TAG "app_wifi"
#define AP_SSID_PREFIX "AI-PASSPORT-"
#define AP_PASSWORD "aipassport"
#define WIFI_HTTP_STACK 8192
#define FETCH_BUFFER_SIZE 4096
#define WWV_URL "https://services.swpc.noaa.gov/text/wwv.txt"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

typedef enum {
    WIFI_CMD_PORTAL = 0,
    WIFI_CMD_PORTAL_SAVED,
    WIFI_CMD_UPDATE,
    WIFI_CMD_UPDATE_LOCATION,
    WIFI_CMD_STOP,
} wifi_cmd_t;

static const char WIFI_FORM_HTML[] =
    "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>AI Passport WiFi 设置</title><style>"
    "body{margin:0;background:#070c16;color:#eaf4f8;font-family:-apple-system,"
    "BlinkMacSystemFont,'Segoe UI',sans-serif;padding:24px}"
    "main{max-width:520px;margin:auto}h1{font-size:22px;margin:0 0 8px}"
    "p{color:#9fb3c8;line-height:1.55}.card{background:#0e1826;border:1px solid "
    "#1d2d40;border-radius:14px;padding:18px;margin-top:18px}"
    "label{display:block;margin:14px 0 6px;color:#9fb3c8}"
    "input{width:100%%;box-sizing:border-box;padding:12px;border-radius:8px;"
    "border:1px solid #1d2d40;background:#0b1420;color:#eaf4f8;font-size:16px}"
    "button{width:100%%;padding:14px;margin-top:18px;border:0;border-radius:10px;"
    "background:#36d6e8;color:#041019;font-size:16px;font-weight:700}"
    "code{color:#8bd450}.scan-head{display:flex;justify-content:space-between;"
    "align-items:center}.scan-head strong{color:#eaf4f8}"
    "button.small{width:auto;margin:0;padding:8px 12px;font-size:14px;"
    "background:#143149;color:#eaf4f8}.nets{max-height:210px;overflow:auto;"
    "margin-top:10px}.net{display:flex;justify-content:space-between;"
    "align-items:center;width:100%%;margin-top:8px;padding:10px 12px;"
    "border:1px solid #1d2d40;border-radius:8px;background:#0b1420;"
    "color:#eaf4f8;font-size:14px;text-align:left}.net small{color:#9fb3c8;"
    "font-size:12px;margin-left:12px;white-space:nowrap}"
    "</style></head><body><main><h1>AI Passport WiFi 设置</h1>"
    "<p>扫描并选择设备要连接的 2.4 GHz WiFi。保存后设备会断开配置热点，"
    "回到设备进入“数据更新”获取 NOAA 实时指数。</p><div class=\"card\">"
    "<div class=\"scan-head\"><strong>附近的 WiFi</strong>"
    "<button type=\"button\" class=\"small\" onclick=\"scan()\">重新扫描</button></div>"
    "<div id=\"networks\" class=\"nets\">正在扫描...</div>"
    "<form method=\"post\" action=\"/save\">"
    "<label>WiFi 名称（SSID）</label>"
    "<input id=\"ssid\" name=\"ssid\" maxlength=\"32\" required autocomplete=\"off\">"
    "<label>WiFi 密码</label>"
    "<input id=\"password\" name=\"password\" type=\"password\" maxlength=\"64\" "
    "autocomplete=\"off\">"
    "<button type=\"submit\">保存 WiFi</button>"
    "</form></div><p>设备配置热点：<code>%s</code>，密码 <code>%s</code>。"
    "</p></main><script>"
    "const ssidInput=document.getElementById('ssid');"
    "const passwordInput=document.getElementById('password');"
    "const networks=document.getElementById('networks');"
    "let scanning=false;"
    "async function scan(retry=0){if(scanning)return;scanning=true;"
    "networks.textContent=retry?'连接中断，正在重试...':'正在扫描...';"
    "const controller=new AbortController();"
    "const timer=setTimeout(()=>controller.abort(),20000);"
    "try{const response=await fetch('/scan',{cache:'no-store',"
    "signal:controller.signal});clearTimeout(timer);"
    "if(!response.ok)throw new Error(await response.text());"
    "const list=await response.json();networks.replaceChildren();"
    "if(list.length===0){networks.textContent='没有发现 2.4 GHz WiFi';"
    "scanning=false;return;}"
    "for(const network of list){const button=document.createElement('button');"
    "button.type='button';button.className='net';"
    "const name=document.createElement('span');name.textContent=network.ssid;"
    "const detail=document.createElement('small');"
    "detail.textContent=network.rssi+' dBm · '+network.auth;"
    "button.append(name,detail);button.addEventListener('click',()=>{"
    "ssidInput.value=network.ssid;passwordInput.focus();});"
    "networks.appendChild(button);}scanning=false;}"
    "catch(error){clearTimeout(timer);if(retry<2){"
    "await new Promise(resolve=>setTimeout(resolve,4000));scanning=false;"
    "return scan(retry+1);}networks.textContent='扫描失败：'+error.message;"
    "scanning=false;}}"
    "scan();</script></body></html>";

static SemaphoreHandle_t s_lock;
static QueueHandle_t s_cmd_queue;
static TaskHandle_t s_task;
static EventGroupHandle_t s_events;
static httpd_handle_t s_server;
static esp_netif_t *s_ap_netif;
static esp_netif_t *s_sta_netif;
static bool s_wifi_initialized;
static bool s_wifi_started;
static app_wifi_status_t s_status;
static bool s_result_pending;
static int s_result_sfi;
static int s_result_kp;
static int s_result_a;
static char s_result_issued[32];
static bool s_location_pending;
static app_location_t s_location_result;
static char s_fetch_buffer[FETCH_BUFFER_SIZE];
static int s_fetch_len;
static esp_event_handler_instance_t s_wifi_handler;
static esp_event_handler_instance_t s_ip_handler;

static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t save_handler(httpd_req_t *req);
static esp_err_t scan_handler(httpd_req_t *req);

static const httpd_uri_t ROOT_URI = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t SAVE_URI = {
    .uri = "/save",
    .method = HTTP_POST,
    .handler = save_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t SCAN_URI = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_handler,
    .user_ctx = NULL,
};

static void status_set(app_wifi_state_t state, const char *message)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    if (message) {
        snprintf(s_status.message, sizeof(s_status.message), "%s", message);
    }
    xSemaphoreGive(s_lock);
}

static void status_set_config(const app_wifi_config_t *config)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.has_config = app_wifi_config_valid(config);
    if (s_status.has_config) {
        snprintf(s_status.router_ssid, sizeof(s_status.router_ssid), "%.32s",
                 config->ssid);
    } else {
        s_status.router_ssid[0] = '\0';
    }
    xSemaphoreGive(s_lock);
}

static void status_set_result(const space_weather_t *weather)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.sfi = weather->sfi;
    s_status.kp = weather->kp;
    s_status.a_index = weather->a_index;
    snprintf(s_status.issued, sizeof(s_status.issued), "%.31s", weather->issued);
    s_result_sfi = weather->sfi;
    s_result_kp = weather->kp;
    s_result_a = weather->a_index;
    snprintf(s_result_issued, sizeof(s_result_issued), "%.31s", weather->issued);
    s_result_pending = true;
    xSemaphoreGive(s_lock);
}

static void status_set_location(const app_location_t *location)
{
    if (!s_lock || !location || !location->valid) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_location_result = *location;
    s_location_pending = true;
    xSemaphoreGive(s_lock);
}

void app_wifi_status(app_wifi_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_status;
    xSemaphoreGive(s_lock);
}

bool app_wifi_take_result(int *sfi, int *kp, int *a_index,
                          char *issued, size_t issued_size)
{
    if (!s_lock) return false;
    bool available = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_result_pending) {
        s_result_pending = false;
        if (sfi) *sfi = s_result_sfi;
        if (kp) *kp = s_result_kp;
        if (a_index) *a_index = s_result_a;
        if (issued && issued_size > 0) {
            snprintf(issued, issued_size, "%s", s_result_issued);
        }
        available = true;
    }
    xSemaphoreGive(s_lock);
    return available;
}

bool app_wifi_take_location(app_location_t *out)
{
    if (!s_lock || !out) return false;
    bool available = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_location_pending) {
        *out = s_location_result;
        s_location_pending = false;
        available = true;
    }
    xSemaphoreGive(s_lock);
    return available;
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupSetBits(s_events, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    }
}

static void stop_radio(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    if (s_wifi_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_wifi_started = false;
    }
    if (s_wifi_initialized) {
        esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event && event->event_id == HTTP_EVENT_ON_DATA &&
        event->data && event->data_len > 0) {
        int available = (int)sizeof(s_fetch_buffer) - 1 - s_fetch_len;
        int copy_len = event->data_len < available ? event->data_len : available;
        if (copy_len > 0) {
            memcpy(s_fetch_buffer + s_fetch_len, event->data, (size_t)copy_len);
            s_fetch_len += copy_len;
        }
    }
    return ESP_OK;
}

#define SCAN_MAX_AP 24
#define SCAN_MAX_RESULTS 15

static wifi_ap_record_t s_scan_records[SCAN_MAX_AP];
static char s_scan_json[3072];
static char s_portal_page[4096];
static volatile bool s_scan_busy;

static const char *auth_text(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN: return "开放";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "企业";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "加密";
    }
}

static void json_escape(const char *source, char *destination, size_t size)
{
    size_t used = 0;
    if (!destination || size == 0) return;
    if (source) {
        for (const unsigned char *cursor = (const unsigned char *)source;
             *cursor && used + 1 < size; cursor++) {
            if (*cursor == '"' || *cursor == '\\') {
                if (used + 2 >= size) break;
                destination[used++] = '\\';
                destination[used++] = (char)*cursor;
            } else if (*cursor < 0x20) {
                if (used + 6 >= size) break;
                snprintf(destination + used, size - used, "\\u%04x", *cursor);
                used += 6;
            } else {
                destination[used++] = (char)*cursor;
            }
        }
    }
    destination[used] = '\0';
}

static esp_err_t scan_handler(httpd_req_t *req)
{
    if (!s_wifi_started) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "hotspot not active");
    }
    if (s_scan_busy) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "scan busy");
    }
    s_scan_busy = true;

    wifi_scan_config_t scan_cfg = {
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 0,
        .scan_time.active.max = 90,
        .home_chan_dwell_time = 120,
        .coex_background_scan = true,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "scan failed: %s", esp_err_to_name(err));
        s_scan_busy = false;
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "scan failed");
    }
    uint16_t count = SCAN_MAX_AP;
    err = esp_wifi_scan_get_ap_records(&count, s_scan_records);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "scan read failed: %s", esp_err_to_name(err));
        s_scan_busy = false;
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "scan read failed");
    }
    ESP_LOGI(TAG, "scan complete: %u APs", (unsigned)count);

    for (uint16_t i = 1; i < count; i++) {
        wifi_ap_record_t record = s_scan_records[i];
        int j = (int)i - 1;
        while (j >= 0 && s_scan_records[j].rssi < record.rssi) {
            s_scan_records[j + 1] = s_scan_records[j];
            j--;
        }
        s_scan_records[j + 1] = record;
    }

    app_wifi_status_t status;
    app_wifi_status(&status);
    char seen[SCAN_MAX_RESULTS][33] = {0};
    size_t used = 0;
    int emitted = 0;
    used += (size_t)snprintf(s_scan_json + used, sizeof(s_scan_json) - used, "[");

    for (uint16_t i = 0; i < count && emitted < SCAN_MAX_RESULTS; i++) {
        char ssid[33];
        size_t length = strnlen((const char *)s_scan_records[i].ssid, 32);
        memcpy(ssid, s_scan_records[i].ssid, length);
        ssid[length] = '\0';
        if (!ssid[0] || strcmp(ssid, status.ap_ssid) == 0) continue;

        bool duplicate = false;
        for (int j = 0; j < emitted; j++) {
            if (strcmp(seen[j], ssid) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        char escaped[160];
        json_escape(ssid, escaped, sizeof(escaped));
        int written = snprintf(
            s_scan_json + used, sizeof(s_scan_json) - used,
            "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":\"%s\"}",
            emitted ? "," : "", escaped, s_scan_records[i].rssi,
            auth_text(s_scan_records[i].authmode));
        if (written < 0 || used + (size_t)written + 2 >= sizeof(s_scan_json)) {
            break;
        }
        used += (size_t)written;
        snprintf(seen[emitted], sizeof(seen[emitted]), "%s", ssid);
        emitted++;
    }

    s_scan_json[used++] = ']';
    s_scan_json[used] = '\0';
    s_scan_busy = false;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, s_scan_json, HTTPD_RESP_USE_STRLEN);
}

static void copy_wifi_field(char *destination, size_t size, const char *source)
{
    if (size == 0) return;
    size_t length = source ? strlen(source) : 0;
    if (length >= size) length = size - 1;
    if (length) memcpy(destination, source, length);
    destination[length] = '\0';
}

static esp_err_t start_ap_portal(void)
{
    if (s_server && s_wifi_started) {
        status_set(APP_WIFI_PORTAL_READY, "热点已开启，等待配置");
        return ESP_OK;
    }
    status_set(APP_WIFI_PORTAL_STARTING, "正在开启热点...");
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_WIFI_AP();
    s_ap_netif = esp_netif_new(&netif_cfg);
    if (!s_ap_netif) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    err = esp_netif_attach_wifi_ap(s_ap_netif);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_default_wifi_ap_handlers();
    if (err != ESP_OK) goto fail;

    esp_netif_config_t sta_cfg = ESP_NETIF_DEFAULT_WIFI_STA();
    s_sta_netif = esp_netif_new(&sta_cfg);
    if (!s_sta_netif) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    err = esp_netif_attach_wifi_station(s_sta_netif);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_default_wifi_sta_handlers();
    if (err != ESP_OK) goto fail;

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) goto fail;
    s_wifi_initialized = true;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) goto fail;

    app_wifi_status_t status;
    app_wifi_status(&status);
    wifi_config_t wifi_cfg = {0};
    copy_wifi_field((char *)wifi_cfg.ap.ssid, sizeof(wifi_cfg.ap.ssid),
                    status.ap_ssid);
    copy_wifi_field((char *)wifi_cfg.ap.password, sizeof(wifi_cfg.ap.password),
                    status.ap_password);
    wifi_cfg.ap.ssid_len = (uint8_t)strlen((char *)wifi_cfg.ap.ssid);
    wifi_cfg.ap.channel = 1;
    wifi_cfg.ap.max_connection = 2;
    wifi_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.ap.pmf_cfg.required = false;
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_start();
    if (err != ESP_OK) goto fail;
    s_wifi_started = true;

    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.max_uri_handlers = 3;
    http_cfg.stack_size = WIFI_HTTP_STACK;
    http_cfg.lru_purge_enable = true;
    http_cfg.recv_wait_timeout = 20;
    err = httpd_start(&s_server, &http_cfg);
    if (err != ESP_OK) goto fail;
    if ((err = httpd_register_uri_handler(s_server, &ROOT_URI)) != ESP_OK) goto fail;
    if ((err = httpd_register_uri_handler(s_server, &SAVE_URI)) != ESP_OK) goto fail;
    if ((err = httpd_register_uri_handler(s_server, &SCAN_URI)) != ESP_OK) goto fail;

    status_set(APP_WIFI_PORTAL_READY, "热点已开启，等待配置");
    ESP_LOGI(TAG, "portal ready: ssid=%s url=http://192.168.4.1/",
             status.ap_ssid);
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "portal start failed: %s", esp_err_to_name(err));
    stop_radio();
    status_set(APP_WIFI_FAILED, "热点开启失败");
    return err;
}

static esp_err_t fetch_weather_once(space_weather_t *weather)
{
    esp_http_client_config_t config = {
        .url = WWV_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .timeout_ms = 20000,
        .buffer_size = 1024,
        .buffer_size_tx = 512,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;
    (void)esp_http_client_set_header(client, "User-Agent", "AI-Passport/1.0");
    s_fetch_len = 0;
    s_fetch_buffer[0] = '\0';

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP fetch failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "HTTP status %d", status);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    s_fetch_buffer[s_fetch_len] = '\0';
    esp_http_client_cleanup(client);

    if (s_fetch_len <= 0 || !space_weather_parse_wwv(s_fetch_buffer, weather)) {
        ESP_LOGE(TAG, "NOAA response parse failed (%d bytes): %.160s",
                 s_fetch_len, s_fetch_buffer);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t fetch_weather(space_weather_t *weather)
{
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) vTaskDelay(pdMS_TO_TICKS(800));
        err = fetch_weather_once(weather);
        if (err == ESP_OK) return ESP_OK;
        ESP_LOGW(TAG, "NOAA fetch attempt %d failed: %s",
                 attempt + 1, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t start_sta_and_fetch(bool fetch_location)
{
    app_wifi_config_t config;
    esp_err_t err = app_wifi_config_load(&config);
    if (err != ESP_OK || !app_wifi_config_valid(&config)) {
        status_set(APP_WIFI_FAILED, "请先设置 WiFi");
        return ESP_ERR_INVALID_STATE;
    }
    status_set_config(&config);
    stop_radio();
    status_set(APP_WIFI_CONNECTING, "正在连接 WiFi...");

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_WIFI_STA();
    s_sta_netif = esp_netif_new(&netif_cfg);
    if (!s_sta_netif) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    err = esp_netif_attach_wifi_station(s_sta_netif);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_default_wifi_sta_handlers();
    if (err != ESP_OK) goto fail;

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) goto fail;
    s_wifi_initialized = true;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) goto fail;

    wifi_config_t wifi_cfg = {0};
    copy_wifi_field((char *)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid),
                    config.ssid);
    copy_wifi_field((char *)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password),
                    config.password);
    wifi_cfg.sta.threshold.authmode =
        config.password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err != ESP_OK) goto fail;

    xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    err = esp_wifi_start();
    if (err != ESP_OK) goto fail;
    s_wifi_started = true;

    EventBits_t bits = xEventGroupWaitBits(
        s_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE,
        pdMS_TO_TICKS(20000));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        status_set(APP_WIFI_FAILED, "WiFi 连接失败");
        stop_radio();
        return ESP_ERR_TIMEOUT;
    }

    if (fetch_location) {
        status_set(APP_WIFI_LOCATING, "正在获取网络位置...");
        app_location_t location;
        if (app_location_fetch(&location) == ESP_OK) {
            status_set_location(&location);
        } else {
            ESP_LOGW(TAG, "network location unavailable");
        }
    }

    status_set(APP_WIFI_FETCHING, "正在获取传播数据...");
    space_weather_t weather;
    err = fetch_weather(&weather);
    if (err == ESP_OK) {
        status_set_result(&weather);
        status_set(APP_WIFI_SUCCESS, "数据已更新");
        ESP_LOGI(TAG, "weather updated: SFI=%d Kp=%d A=%d issued=%s",
                 weather.sfi, weather.kp, weather.a_index, weather.issued);
    } else {
        status_set(APP_WIFI_FAILED, "数据获取失败");
    }
    stop_radio();
    return err;

fail:
    ESP_LOGE(TAG, "STA start failed: %s", esp_err_to_name(err));
    stop_radio();
    status_set(APP_WIFI_FAILED, "WiFi 连接失败");
    return err;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *text)
{
    char *read = text;
    char *write = text;
    while (*read) {
        if (*read == '+') {
            *write++ = ' ';
            read++;
        } else if (*read == '%' && hex_value(read[1]) >= 0 &&
                   hex_value(read[2]) >= 0) {
            *write++ = (char)((hex_value(read[1]) << 4) | hex_value(read[2]));
            read += 3;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

static bool form_value(const char *body, const char *key,
                       char *out, size_t out_size)
{
    if (!body || !key || !out || out_size == 0) return false;
    size_t key_len = strlen(key);
    const char *cursor = body;
    while (cursor && *cursor) {
        if (strncmp(cursor, key, key_len) == 0 && cursor[key_len] == '=') {
            cursor += key_len + 1;
            size_t used = 0;
            while (*cursor && *cursor != '&' && used + 1 < out_size) {
                out[used++] = *cursor++;
            }
            out[used] = '\0';
            return true;
        }
        cursor = strchr(cursor, '&');
        if (cursor) cursor++;
    }
    return false;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    app_wifi_status_t status;
    app_wifi_status(&status);
    snprintf(s_portal_page, sizeof(s_portal_page), WIFI_FORM_HTML, status.ap_ssid,
             status.ap_password);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, s_portal_page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char body[512];
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
    }
    body[received] = '\0';

    app_wifi_config_t config = {0};
    if (!form_value(body, "ssid", config.ssid, sizeof(config.ssid))) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
    }
    (void)form_value(body, "password", config.password,
                     sizeof(config.password));
    url_decode(config.ssid);
    url_decode(config.password);
    if (!app_wifi_config_valid(&config)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid WiFi");
    }

    esp_err_t err = app_wifi_config_save(&config);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "save failed");
    }
    status_set_config(&config);
    status_set(APP_WIFI_CONFIG_SAVED, "WiFi 已保存");
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_send(req, "已保存，请回到设备进入数据更新", HTTPD_RESP_USE_STRLEN);
    wifi_cmd_t cmd = WIFI_CMD_PORTAL_SAVED;
    (void)xQueueSend(s_cmd_queue, &cmd, 0);
    return ESP_OK;
}

static void wifi_task(void *arg)
{
    (void)arg;
    wifi_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd) {
            case WIFI_CMD_PORTAL:
                (void)start_ap_portal();
                break;
            case WIFI_CMD_PORTAL_SAVED:
                stop_radio();
                status_set(APP_WIFI_CONFIG_SAVED, "WiFi 已保存");
                break;
            case WIFI_CMD_UPDATE:
                (void)start_sta_and_fetch(false);
                break;
            case WIFI_CMD_UPDATE_LOCATION:
                (void)start_sta_and_fetch(true);
                break;
            case WIFI_CMD_STOP:
                stop_radio();
                status_set(APP_WIFI_IDLE, "已停止");
                break;
            }
        }
    }
}

esp_err_t app_wifi_init(void)
{
    if (s_task) return ESP_OK;
    esp_err_t err = app_wifi_config_init();
    if (err != ESP_OK) return err;
    s_lock = xSemaphoreCreateMutex();
    s_cmd_queue = xQueueCreate(4, sizeof(wifi_cmd_t));
    s_events = xEventGroupCreate();
    if (!s_lock || !s_cmd_queue || !s_events) return ESP_ERR_NO_MEM;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &s_wifi_handler);
    if (err != ESP_OK) return err;
    err = esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &s_ip_handler);
    if (err != ESP_OK) return err;

    memset(&s_status, 0, sizeof(s_status));
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) != ESP_OK) memset(mac, 0, sizeof(mac));
    snprintf(s_status.ap_ssid, sizeof(s_status.ap_ssid),
             AP_SSID_PREFIX "%02X%02X%02X", mac[3], mac[4], mac[5]);
    snprintf(s_status.ap_password, sizeof(s_status.ap_password), "%s",
             AP_PASSWORD);

    app_wifi_config_t config;
    if (app_wifi_config_load(&config) == ESP_OK && app_wifi_config_valid(&config)) {
        s_status.has_config = true;
        snprintf(s_status.router_ssid, sizeof(s_status.router_ssid), "%.32s",
                 config.ssid);
        snprintf(s_status.message, sizeof(s_status.message), "%s",
                 "按 OK 更新数据");
    } else {
        snprintf(s_status.message, sizeof(s_status.message), "%s",
                 "请先设置 WiFi");
    }
    s_status.state = APP_WIFI_IDLE;

    if (xTaskCreate(wifi_task, "app_wifi", 12288, NULL, 4, &s_task) != pdPASS) {
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_wifi_start_portal(void)
{
    if (!s_cmd_queue) return ESP_ERR_INVALID_STATE;
    status_set(APP_WIFI_PORTAL_STARTING, "正在开启热点...");
    wifi_cmd_t cmd = WIFI_CMD_PORTAL;
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t app_wifi_start_update(bool fetch_location)
{
    if (!s_cmd_queue) return ESP_ERR_INVALID_STATE;
    app_wifi_config_t config;
    if (app_wifi_config_load(&config) != ESP_OK ||
        !app_wifi_config_valid(&config)) {
        status_set(APP_WIFI_FAILED, "请先设置 WiFi");
        return ESP_ERR_INVALID_STATE;
    }
    status_set(APP_WIFI_CONNECTING, "正在连接 WiFi...");
    wifi_cmd_t cmd = fetch_location ? WIFI_CMD_UPDATE_LOCATION : WIFI_CMD_UPDATE;
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t app_wifi_request_stop(void)
{
    if (!s_cmd_queue) return ESP_ERR_INVALID_STATE;
    wifi_cmd_t cmd = WIFI_CMD_STOP;
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

