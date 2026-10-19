#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "app_settings.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "prop_model.h"

static int failures;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,         \
                    #condition);                                             \
            failures++;                                                      \
        }                                                                    \
    } while (0)

int main(void)
{
    CHECK(nvs_flash_erase() == ESP_OK);
    CHECK(app_settings_init() == ESP_OK);

    app_settings_t settings;
    app_settings_defaults(&settings);
    CHECK(settings.station == 0);
    CHECK(settings.target == 0);
    CHECK(settings.utc_offset_min == 480);
    CHECK(settings.sfi == 150);
    CHECK(settings.kp == 2);
    CHECK(!settings.has_location);

    settings.station = PROP_STATION_AUTO;
    CHECK(app_settings_save(&settings) == ESP_OK);
    app_settings_t loaded;
    CHECK(app_settings_load(&loaded) == ESP_OK);
    CHECK(loaded.station == PROP_STATION_AUTO);
    CHECK(!loaded.has_location);

    app_settings_set_location(&settings, 31.2304f, 121.4737f);
    CHECK(app_settings_save(&settings) == ESP_OK);
    CHECK(app_settings_load(&loaded) == ESP_OK);
    CHECK(loaded.has_location);
    CHECK(fabsf(loaded.latitude - 31.2304f) < 0.0001f);
    CHECK(fabsf(loaded.longitude - 121.4737f) < 0.0001f);

    settings.station = 0;
    app_settings_cycle_station(&settings, -1);
    CHECK(settings.station == PROP_STATION_COUNT - 1);
    app_settings_cycle_station(&settings, 1);
    CHECK(settings.station == 0);

    app_settings_set_location(&settings, 39.9042f, 116.4074f);
    CHECK(settings.has_location);
    CHECK(settings.latitude > 39.8f && settings.latitude < 40.0f);
    CHECK(settings.longitude > 116.3f && settings.longitude < 116.5f);
    app_settings_set_location(&settings, 91.0f, 0.0f);
    CHECK(!settings.has_location);

    app_settings_cycle_target(&settings, -1);
    CHECK(settings.target == PROP_TARGET_COUNT - 1);
    app_settings_cycle_target(&settings, 1);
    CHECK(settings.target == 0);

    app_settings_adjust_offset(&settings, -2000);
    CHECK(settings.utc_offset_min == -720);
    app_settings_adjust_offset(&settings, 4000);
    CHECK(settings.utc_offset_min == 840);

    app_settings_adjust_sfi(&settings, -1000);
    CHECK(settings.sfi == 70);
    app_settings_adjust_sfi(&settings, 1000);
    CHECK(settings.sfi == 250);
    app_settings_adjust_sfi(&settings, 5);
    CHECK(settings.sfi == 250);

    app_settings_adjust_kp(&settings, -5);
    CHECK(settings.kp == 0);
    app_settings_adjust_kp(&settings, 20);
    CHECK(settings.kp == 9);
    settings.has_location = true;
    settings.latitude = 200.0f;
    settings.longitude = 10.0f;
    app_settings_validate(&settings);
    CHECK(!settings.has_location);

    settings.station = 200;
    settings.target = 200;
    settings.utc_offset_min = -3000;
    settings.sfi = 5;
    settings.kp = 200;
    app_settings_validate(&settings);
    CHECK(settings.station == 0);
    CHECK(settings.target == 0);
    CHECK(settings.utc_offset_min == -720);
    CHECK(settings.sfi == 70);
    CHECK(settings.kp == 9);

    if (failures != 0) {
        fprintf(stderr, "%d checks failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("app_settings tests: PASS\n");
    return EXIT_SUCCESS;
}

