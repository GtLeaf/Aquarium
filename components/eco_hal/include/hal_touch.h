#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOUCH_EVT_NONE = 0,
    TOUCH_EVT_PRESS,
    TOUCH_EVT_RELEASE,
    TOUCH_EVT_MOVE,
    TOUCH_EVT_LONG_PRESS,
} touch_event_type_t;

typedef struct {
    touch_event_type_t type;
    int16_t x;
    int16_t y;
} touch_event_t;

esp_err_t hal_touch_init(void);
bool hal_touch_read(int16_t *x, int16_t *y);
bool hal_touch_get_event(touch_event_t *event);

#ifdef __cplusplus
}
#endif
