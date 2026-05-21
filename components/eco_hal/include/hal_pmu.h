#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_pmu_init(void);
uint8_t hal_pmu_get_battery_percent(void);
bool hal_pmu_is_charging(void);

#ifdef __cplusplus
}
#endif
