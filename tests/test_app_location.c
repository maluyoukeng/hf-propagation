#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_location_parse.h"

static int failures;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,         \
                    #condition);                                             \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static void expect_location(const char *json, float latitude, float longitude,
                            const char *city)
{
    app_location_t location;
    CHECK(app_location_parse(json, &location));
    CHECK(location.valid);
    CHECK(fabsf(location.latitude - latitude) < 0.001f);
    CHECK(fabsf(location.longitude - longitude) < 0.001f);
    CHECK(strcmp(location.city, city) == 0);
}

int main(void)
{
    expect_location(
        "{\"ip\":\"203.0.113.1\",\"success\":true,\"country\":\"China\","
        "\"region\":\"Beijing\",\"city\":\"Beijing\",\"latitude\":39.9042,"
        "\"longitude\":116.4074,\"timezone\":{\"id\":\"Asia/Shanghai\"}}",
        39.9042f, 116.4074f, "Beijing");
    expect_location(
        "{\"ip\":\"203.0.113.1\",\"city\":\"Shanghai\",\"region\":\"Shanghai\","
        "\"country_name\":\"China\",\"latitude\":31.2304,\"longitude\":121.4737,"
        "\"timezone\":\"Asia/Shanghai\"}",
        31.2304f, 121.4737f, "Shanghai");
    expect_location(
        "{\"status\":\"success\",\"country\":\"China\",\"regionName\":\"Guangdong\","
        "\"city\":\"Shenzhen\",\"lat\":22.5455,\"lon\":114.0544}",
        22.5455f, 114.0544f, "Shenzhen");
    expect_location(
        "{\"ipVersion\":4,\"latitude\":39.9042,\"longitude\":116.4074,"
        "\"countryName\":\"China\",\"cityName\":\"Beijing\"}",
        39.9042f, 116.4074f, "Beijing");

    app_location_t location;
    CHECK(!app_location_parse(
        "{\"status\":\"fail\",\"message\":\"private range\"}", &location));
    CHECK(!app_location_parse("{\"success\":false,\"message\":\"reserved\"}",
                              &location));
    CHECK(!app_location_parse("{\"city\":\"Nowhere\"}", &location));
    CHECK(!app_location_parse(
        "{\"latitude\":95.0,\"longitude\":116.0}", &location));
    CHECK(!app_location_parse("{\"latitude\":0.0,\"longitude\":0.0}",
                              &location));
    CHECK(!app_location_parse(NULL, &location));
    CHECK(!app_location_parse("{}", NULL));

    CHECK(app_location_parse(
        "{\"latitude\":-33.8688,\"longitude\":151.2093,\"city\":\"Sydney\"}",
        &location));
    CHECK(location.latitude < 0.0f && location.longitude > 0.0f);

    if (failures != 0) {
        fprintf(stderr, "%d checks failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("app_location tests: PASS\n");
    return EXIT_SUCCESS;
}
