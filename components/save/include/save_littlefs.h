#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t save_littlefs_init(void);
esp_err_t save_littlefs_write(const char *path, const void *data, size_t len);
esp_err_t save_littlefs_read(const char *path, void *data, size_t len);

#ifdef __cplusplus
}
#endif
