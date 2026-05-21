#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_rtc_init(void);
struct tm hal_rtc_get_time(void);
void hal_rtc_set_time(const struct tm *timeinfo);
bool hal_rtc_is_daytime(void); // 6:00 - 18:00

#ifdef __cplusplus
}
#endif
