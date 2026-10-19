#include "app_clock.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_timer.h"
#include "nvs.h"
#include "prop_model.h"

#define CLOCK_NAMESPACE "hfprop"
#define CLOCK_KEY "clock"
#define CLOCK_MIN_VALID_EPOCH 1577836800LL
#define CLOCK_PERSIST_INTERVAL_US (300LL * 1000LL * 1000LL)

static int64_t s_base_epoch;
static int64_t s_base_us;
static int64_t s_last_persist_us;
static nvs_handle_t s_nvs;
static bool s_nvs_ready;

static int parse_month(const char *text)
{
    static const char *const months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    for (int i = 0; i < 12; i++) {
        if (strncmp(text, months[i], 3) == 0) return i + 1;
    }
    return 1;
}

static int64_t build_epoch(void)
{
    const char date[] = __DATE__;
    const char time[] = __TIME__;
    const int month = parse_month(date);
    const int day = (date[4] == ' ') ? date[5] - '0'
                                     : (date[4] - '0') * 10 + (date[5] - '0');
    const int year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 +
                     (date[9] - '0') * 10 + (date[10] - '0');
    const int hour = (time[0] - '0') * 10 + (time[1] - '0');
    const int minute = (time[3] - '0') * 10 + (time[4] - '0');
    const int second = (time[6] - '0') * 10 + (time[7] - '0');
    return prop_epoch_from_utc(year, (unsigned)month, (unsigned)day,
                               (unsigned)hour, (unsigned)minute,
                               (unsigned)second);
}

void app_clock_init(void)
{
    s_base_epoch = build_epoch();
    s_base_us = esp_timer_get_time();
    s_last_persist_us = s_base_us;
    s_nvs_ready = false;

    esp_err_t err = nvs_open(CLOCK_NAMESPACE, NVS_READWRITE, &s_nvs);
    if (err == ESP_OK) {
        s_nvs_ready = true;
        int64_t stored = 0;
        if (nvs_get_i64(s_nvs, CLOCK_KEY, &stored) == ESP_OK &&
            stored >= CLOCK_MIN_VALID_EPOCH && stored > s_base_epoch) {
            s_base_epoch = stored;
        }
    }
}

int64_t app_clock_now(void)
{
    const int64_t elapsed_us = esp_timer_get_time() - s_base_us;
    return s_base_epoch + elapsed_us / 1000000LL;
}

bool app_clock_valid(void)
{
    return app_clock_now() >= CLOCK_MIN_VALID_EPOCH;
}

void app_clock_set_epoch(int64_t epoch)
{
    s_base_epoch = epoch;
    s_base_us = esp_timer_get_time();
    s_last_persist_us = 0;
}

void app_clock_adjust(int64_t delta_seconds)
{
    app_clock_set_epoch(app_clock_now() + delta_seconds);
}

void app_clock_persist_if_due(void)
{
    if (!s_nvs_ready) return;
    const int64_t now_us = esp_timer_get_time();
    if (now_us - s_last_persist_us < CLOCK_PERSIST_INTERVAL_US) return;
    s_last_persist_us = now_us;
    if (nvs_set_i64(s_nvs, CLOCK_KEY, app_clock_now()) == ESP_OK) {
        (void)nvs_commit(s_nvs);
    }
}

