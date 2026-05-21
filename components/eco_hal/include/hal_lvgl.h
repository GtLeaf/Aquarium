#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_lvgl_init(void);
void hal_lvgl_port_task(void *arg);

#ifdef __cplusplus
}
#endif
