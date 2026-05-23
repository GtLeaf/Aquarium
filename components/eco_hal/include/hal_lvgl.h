#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hal_lvgl_init(void);
void hal_lvgl_port_task(void *arg);
void hal_lvgl_set_ui_update_cb(void (*cb)(void));
void hal_lvgl_set_interaction_cb(void (*cb)(void));

#ifdef __cplusplus
}
#endif
