#pragma once

#include "app_location_parse.h"
#include "esp_err.h"

esp_err_t app_location_fetch(app_location_t *out);
