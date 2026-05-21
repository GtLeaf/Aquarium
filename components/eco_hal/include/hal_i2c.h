#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t hal_i2c_init(void);
esp_err_t hal_i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *data);
esp_err_t hal_i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t data);
bool hal_i2c_is_initialized(void);

#endif
