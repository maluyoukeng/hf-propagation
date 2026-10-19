#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_spaceweather.h"

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
    const char *sample =
        ":Product: Geophysical Alert Message wwv.txt\n"
        ":Issued: 2026 Oct 19 0605 UTC\n"
        "Solar flux 137 and estimated planetary A-index 6.\n"
        "The estimated planetary K-index at 0600 UTC on 19 October was 0.33.\n";
    space_weather_t data;
    CHECK(space_weather_parse_wwv(sample, &data));
    CHECK(data.sfi == 137);
    CHECK(data.a_index == 6);
    CHECK(data.kp == 0);
    CHECK(strcmp(data.issued, "2026 Oct 19 0605 UTC") == 0);

    const char *fractional =
        "Solar flux 180 and estimated planetary A-index 22.\n"
        "The estimated planetary K-index at 0900 UTC on 20 October was 4.6.\n";
    CHECK(space_weather_parse_wwv(fractional, &data));
    CHECK(data.sfi == 180);
    CHECK(data.a_index == 22);
    CHECK(data.kp == 5);

    CHECK(!space_weather_parse_wwv("no indices here", &data));
    CHECK(!space_weather_parse_wwv(NULL, &data));

    if (failures != 0) {
        fprintf(stderr, "%d checks failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("space weather parser tests: PASS\n");
    return EXIT_SUCCESS;
}

