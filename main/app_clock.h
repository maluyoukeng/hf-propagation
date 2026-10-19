#pragma once

#include <stdbool.h>
#include <stdint.h>

void app_clock_init(void);
int64_t app_clock_now(void);
bool app_clock_valid(void);
void app_clock_set_epoch(int64_t epoch);
void app_clock_adjust(int64_t delta_seconds);
void app_clock_persist_if_due(void);

