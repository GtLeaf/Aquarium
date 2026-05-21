#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_WIDTH   368
#define DISPLAY_HEIGHT  448
#define DISPLAY_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

esp_err_t hal_display_init(void);
void hal_display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void hal_display_set_brightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif