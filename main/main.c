#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"

#include "app_clock.h"
#include "app_settings.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "hf_app_ui.h"
#include "key_guard.h"
#include "prop_model.h"

#define TAG "hf_prop"
#define INPUT_QUEUE_DEPTH 8
#define APP_TICK_MS 500
#define BATTERY_SAMPLE_US (10LL * 1000 * 1000)
#define SAVED_NOTICE_US (1500000LL)
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static TaskHandle_t s_app_task;
static volatile bool s_input_ready;
static key_guard_t s_key_guard;

static hf_view_t s_view;
static app_settings_t s_edit_backup;
static int64_t s_edit_clock_backup;
static int64_t s_saved_until_us;
static int64_t s_last_battery_us;
static int64_t s_last_minute_epoch = -1;
static bool s_battery_available;
static bool s_dirty = true;
static int s_scroll_pending;

static bool wifi_status_differs(const app_wifi_status_t *a,
                                const app_wifi_status_t *b)
{
    return a->state != b->state || a->has_config != b->has_config ||
           a->sfi != b->sfi || a->kp != b->kp || a->a_index != b->a_index ||
           strcmp(a->ap_ssid, b->ap_ssid) != 0 ||
           strcmp(a->ap_password, b->ap_password) != 0 ||
           strcmp(a->router_ssid, b->router_ssid) != 0 ||
           strcmp(a->issued, b->issued) != 0 ||
           strcmp(a->message, b->message) != 0;
}

static int battery_percent_read(void)
{
    if (!s_battery_available) return -1;
    return bsp_battery_soc();
}

static void build_prop_input(prop_input_t *input)
{
    input->station = s_view.settings.station;
    input->target = s_view.settings.target;
    input->utc_offset_min = s_view.settings.utc_offset_min;
    input->sfi = s_view.settings.sfi;
    input->kp = s_view.settings.kp;
    input->has_location = s_view.settings.has_location;
    input->latitude = s_view.settings.latitude;
    input->longitude = s_view.settings.longitude;
}

static void refresh_forecast(bool include_window)
{
    s_view.epoch = app_clock_now();
    prop_input_t input;
    build_prop_input(&input);
    if (!prop_forecast(&input, s_view.epoch, &s_view.forecast)) {
        ESP_LOGW(TAG, "forecast input rejected");
        memset(&s_view.forecast, 0, sizeof(s_view.forecast));
    }
    if (include_window) {
        if (!prop_best_window(&input, s_view.epoch, s_view.detail_band,
                              &s_view.window)) {
            memset(&s_view.window, 0, sizeof(s_view.window));
        }
    }
}

static void persist_settings(void)
{
    s_view.settings.clock_epoch = app_clock_now();
    esp_err_t err = app_settings_save(&s_view.settings);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "settings save failed: %s", esp_err_to_name(err));
        s_saved_until_us = 0;
    } else {
        s_saved_until_us = esp_timer_get_time() + SAVED_NOTICE_US;
    }
}

static void settings_enter_edit(void)
{
    s_edit_backup = s_view.settings;
    s_edit_clock_backup = app_clock_now();
    s_view.editing = true;
    s_view.saved_notice = false;
}

static void settings_cancel_edit(void)
{
    const bool clock_differs = s_view.settings.clock_epoch != s_edit_backup.clock_epoch;
    s_view.settings = s_edit_backup;
    if (s_view.settings_index == APP_SETTING_TIME ||
        s_view.settings_index == APP_SETTING_DATE || clock_differs) {
        app_clock_set_epoch(s_edit_clock_backup);
    }
    s_view.editing = false;
    s_view.saved_notice = false;
    refresh_forecast(false);
}

static void adjust_setting(int direction)
{
    switch (s_view.settings_index) {
    case APP_SETTING_STATION:
        app_settings_cycle_station(&s_view.settings, direction);
        break;
    case APP_SETTING_TARGET:
        app_settings_cycle_target(&s_view.settings, direction);
        break;
    case APP_SETTING_UTC_OFFSET:
        app_settings_adjust_offset(&s_view.settings, direction * 30);
        break;
    case APP_SETTING_SFI:
        app_settings_adjust_sfi(&s_view.settings, direction * 5);
        break;
    case APP_SETTING_KP:
        app_settings_adjust_kp(&s_view.settings, direction);
        break;
    case APP_SETTING_TIME:
        app_clock_adjust((int64_t)direction * 15 * 60);
        break;
    case APP_SETTING_DATE:
        app_clock_adjust((int64_t)direction * 86400);
        break;
    default:
        break;
    }
    refresh_forecast(false);
}

static void open_detail(void)
{
    s_view.detail_band = s_view.band_index;
    s_view.page = HF_PAGE_DETAIL;
    refresh_forecast(true);
}

static void handle_click(bsp_btn_t btn)
{
    const int direction = btn == BSP_BTN_UP ? -1 : (btn == BSP_BTN_DOWN ? 1 : 0);

    switch (s_view.page) {
    case HF_PAGE_OVERVIEW:
        if (direction != 0) {
            s_view.band_index =
                (s_view.band_index + direction + PROP_BAND_COUNT) % PROP_BAND_COUNT;
        } else if (btn == BSP_BTN_OK) {
            open_detail();
        }
        break;
    case HF_PAGE_DETAIL:
        if (direction != 0) {
            s_view.detail_band =
                (s_view.detail_band + direction + PROP_BAND_COUNT) % PROP_BAND_COUNT;
            refresh_forecast(true);
        } else if (btn == BSP_BTN_OK) {
            s_view.page = HF_PAGE_OVERVIEW;
        }
        break;
    case HF_PAGE_MENU:
        if (direction != 0) {
            s_view.menu_index = (s_view.menu_index + direction + 5) % 5;
        } else if (btn == BSP_BTN_OK) {
            if (s_view.menu_index == 0) {
                s_view.page = HF_PAGE_OVERVIEW;
                refresh_forecast(false);
            } else if (s_view.menu_index == 1) {
                s_view.page = HF_PAGE_SETTINGS;
                s_view.editing = false;
                s_view.saved_notice = false;
            } else if (s_view.menu_index == 2) {
                s_view.page = HF_PAGE_ABOUT;
            } else if (s_view.menu_index == 3) {
                s_view.page = HF_PAGE_WIFI;
                app_wifi_status(&s_view.wifi);
            } else {
                s_view.page = HF_PAGE_DATA;
                app_wifi_status(&s_view.wifi);
            }
        }
        break;
    case HF_PAGE_SETTINGS:
        if (direction != 0) {
            if (s_view.editing) {
                adjust_setting(direction);
            } else {
                s_view.settings_index =
                    (s_view.settings_index + direction + APP_SETTING_COUNT) %
                    APP_SETTING_COUNT;
                s_view.saved_notice = false;
            }
        } else if (btn == BSP_BTN_OK) {
            if (s_view.editing) {
                s_view.editing = false;
                persist_settings();
                refresh_forecast(false);
            } else {
                settings_enter_edit();
            }
        }
        break;
    case HF_PAGE_ABOUT:
        if (direction != 0) {
            s_scroll_pending += direction * 48;
        } else if (btn == BSP_BTN_OK) {
            s_view.page = HF_PAGE_MENU;
        }
        break;
    case HF_PAGE_WIFI:
        if (btn == BSP_BTN_OK) {
            app_wifi_status(&s_view.wifi);
            if (s_view.wifi.state == APP_WIFI_PORTAL_STARTING ||
                s_view.wifi.state == APP_WIFI_CONNECTING ||
                s_view.wifi.state == APP_WIFI_LOCATING ||
                s_view.wifi.state == APP_WIFI_FETCHING) {
                break;
            }
            if (s_view.wifi.state == APP_WIFI_PORTAL_READY) {
                (void)app_wifi_request_stop();
            } else {
                (void)app_wifi_start_portal();
            }
            app_wifi_status(&s_view.wifi);
        }
        break;
    case HF_PAGE_DATA:
        if (btn == BSP_BTN_OK) {
            app_wifi_status(&s_view.wifi);
            if (!s_view.wifi.has_config) {
                s_view.page = HF_PAGE_WIFI;
            } else if (s_view.wifi.state == APP_WIFI_CONNECTING ||
                       s_view.wifi.state == APP_WIFI_LOCATING ||
                       s_view.wifi.state == APP_WIFI_FETCHING) {
                break;
            } else {
                (void)app_wifi_start_update(
                    s_view.settings.station == PROP_STATION_AUTO ||
                    !s_view.settings.has_location);
                app_wifi_status(&s_view.wifi);
            }
        }
        break;
    }
    s_dirty = true;
}

static void handle_long_ok(void)
{
    switch (s_view.page) {
    case HF_PAGE_OVERVIEW:
        s_view.page = HF_PAGE_MENU;
        s_view.menu_index = 0;
        break;
    case HF_PAGE_DETAIL:
        s_view.page = HF_PAGE_OVERVIEW;
        break;
    case HF_PAGE_MENU:
        s_view.page = HF_PAGE_OVERVIEW;
        break;
    case HF_PAGE_SETTINGS:
        if (s_view.editing) {
            settings_cancel_edit();
        } else {
            persist_settings();
            s_view.page = HF_PAGE_OVERVIEW;
            refresh_forecast(false);
        }
        break;
    case HF_PAGE_ABOUT:
        s_view.page = HF_PAGE_MENU;
        break;
    case HF_PAGE_WIFI:
    case HF_PAGE_DATA:
        (void)app_wifi_request_stop();
        s_view.page = HF_PAGE_MENU;
        break;
    }
    s_dirty = true;
}

static void handle_input(const input_event_t *input)
{
    if (input->event == BSP_BTN_CLICK) {
        handle_click(input->btn);
    } else if (input->event == BSP_BTN_LONG && input->btn == BSP_BTN_OK) {
        handle_long_ok();
    }
}

static void app_task(void *arg)
{
    (void)arg;
    s_last_battery_us = esp_timer_get_time();
    s_view.battery_percent = battery_percent_read();
    refresh_forecast(false);

    for (;;) {
        input_event_t input;
        bool changed = false;

        if (xQueueReceive(s_input_queue, &input, pdMS_TO_TICKS(APP_TICK_MS)) == pdTRUE) {
            handle_input(&input);
            changed = true;
        }

        const int64_t now_us = esp_timer_get_time();
        const int64_t epoch = app_clock_now();
        if (epoch / 60 != s_last_minute_epoch) {
            s_last_minute_epoch = epoch / 60;
            refresh_forecast(s_view.page == HF_PAGE_DETAIL);
            changed = true;
        }

        if (now_us - s_last_battery_us >= BATTERY_SAMPLE_US) {
            s_last_battery_us = now_us;
            const int percent = battery_percent_read();
            if (percent != s_view.battery_percent) {
                s_view.battery_percent = percent;
                changed = true;
            }
        }

        if (s_view.page == HF_PAGE_WIFI || s_view.page == HF_PAGE_DATA) {
            app_wifi_status_t status;
            app_wifi_status(&status);
            if (wifi_status_differs(&status, &s_view.wifi)) {
                s_view.wifi = status;
                changed = true;
            }
        }

        int fresh_sfi = 0;
        int fresh_kp = 0;
        int fresh_a = 0;
        char fresh_issued[32];
        if (app_wifi_take_result(&fresh_sfi, &fresh_kp, &fresh_a,
                                 fresh_issued, sizeof(fresh_issued))) {
            s_view.settings.sfi = (uint16_t)fresh_sfi;
            s_view.settings.kp = (uint8_t)fresh_kp;
            app_settings_validate(&s_view.settings);
            persist_settings();
            refresh_forecast(false);
            ESP_LOGI(TAG, "applied space weather: SFI=%d Kp=%d A=%d issued=%s",
                     fresh_sfi, fresh_kp, fresh_a, fresh_issued);
            changed = true;
        }

        app_location_t fresh_location;
        if (app_wifi_take_location(&fresh_location)) {
            app_settings_set_location(&s_view.settings,
                                      fresh_location.latitude,
                                      fresh_location.longitude);
            persist_settings();
            if (s_view.settings.station == PROP_STATION_AUTO) {
                refresh_forecast(false);
            }
            ESP_LOGI(TAG, "applied location: %.4f,%.4f city=%s region=%s",
                     (double)fresh_location.latitude,
                     (double)fresh_location.longitude,
                     fresh_location.city, fresh_location.region);
            changed = true;
        }

        if (s_saved_until_us != 0 && now_us >= s_saved_until_us) {
            s_saved_until_us = 0;
            s_view.saved_notice = false;
            changed = true;
        } else if (s_saved_until_us != 0) {
            s_view.saved_notice = true;
        }

        app_clock_persist_if_due();

        if ((changed || s_dirty) && bsp_lvgl_lock(300)) {
            hf_ui_render(&s_view);
            if (s_scroll_pending != 0) {
                hf_ui_scroll(s_scroll_pending);
                s_scroll_pending = 0;
            }
            bsp_lvgl_unlock();
            s_dirty = false;
        }
    }
}

static esp_err_t input_dispatch_init(void)
{
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(app_task, "hf_app", 8192, NULL, 5, &s_app_task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    const bool was_armed = s_key_guard.armed;
    if (key_guard_consume(&s_key_guard, ev == BSP_BTN_RELEASE,
                          esp_timer_get_time())) {
        if (was_armed && !s_key_guard.armed) {
            ESP_LOGI(TAG, "wake key released; input restored");
        }
        return;
    }
    if (!s_input_ready || !s_input_queue) return;
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG) return;
    const input_event_t input = {.btn = btn, .event = ev};
    (void)xQueueSend(s_input_queue, &input, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "shortwave propagation app starting");

    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & ~(1u << ESP_SLEEP_WAKEUP_UNDEFINED)) {
        ESP_LOGI(TAG, "wake cause mask: 0x%02x", (unsigned)wake_causes);
    }
    if (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
    }

    esp_err_t err = app_settings_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
    }
    app_clock_init();

    app_settings_t settings;
    if (app_settings_load(&settings) != ESP_OK) {
        app_settings_defaults(&settings);
    }
    memset(&s_view, 0, sizeof(s_view));
    s_view.settings = settings;
    s_view.page = HF_PAGE_OVERVIEW;
    s_view.battery_percent = -1;
    s_view.band_index = 3;
    s_view.detail_band = 3;
    s_view.epoch = app_clock_now();
    err = app_wifi_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "app wifi init failed: %s", esp_err_to_name(err));
    }
    app_wifi_status(&s_view.wifi);

    (void)bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed");
        return;
    }
    bsp_display_backlight(100);
    s_battery_available = bsp_battery_init() == ESP_OK;
    s_view.battery_percent = battery_percent_read();

    if (input_dispatch_init() != ESP_OK) {
        ESP_LOGE(TAG, "input task init failed");
        return;
    }
    err = bsp_button_init(on_key, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "button init failed: %s", esp_err_to_name(err));
    } else if (s_key_guard.armed) {
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
    }

    if (bsp_lvgl_lock(1000)) {
        hf_ui_render(&s_view);
        bsp_lvgl_unlock();
        s_input_ready = true;
    }

    ESP_LOGI(TAG, "ready: station=%s target=%s sfi=%u kp=%u",
             PROP_STATIONS[s_view.settings.station].name,
             PROP_TARGETS[s_view.settings.target].name,
             (unsigned)s_view.settings.sfi, (unsigned)s_view.settings.kp);
    if (s_view.settings.has_location) {
        ESP_LOGI(TAG, "saved location: %.4f,%.4f",
                 (double)s_view.settings.latitude,
                 (double)s_view.settings.longitude);
    } else if (s_view.settings.station == PROP_STATION_AUTO) {
        ESP_LOGI(TAG, "auto location pending; update data while online");
    }
}

