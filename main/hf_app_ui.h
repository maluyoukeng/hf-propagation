#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_settings.h"
#include "app_wifi.h"
#include "prop_model.h"

typedef enum {
    HF_PAGE_OVERVIEW = 0,
    HF_PAGE_MENU,
    HF_PAGE_DETAIL,
    HF_PAGE_ABOUT,
    HF_PAGE_SETTINGS,
    HF_PAGE_WIFI,
    HF_PAGE_DATA,
} hf_page_t;

typedef struct {
    hf_page_t page;
    int menu_index;
    int band_index;
    int detail_band;
    int settings_index;
    bool editing;
    bool saved_notice;
    int battery_percent;
    int64_t epoch;
    app_settings_t settings;
    prop_forecast_t forecast;
    prop_window_t window;
    app_wifi_status_t wifi;
} hf_view_t;

void hf_ui_render(const hf_view_t *view);
void hf_ui_scroll(int dy);

