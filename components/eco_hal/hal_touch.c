#include "hal_touch.h"
#include "hal_i2c.h"
#include "hal_display.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "hal_touch";

// FT3168 触摸屏引脚定义 (ESP32-S3-Touch-AMOLED-1.8)
// 参考: https://github.com/espressif/arduino-esp32/blob/3.3.1/variants/waveshare_esp32_s3_touch_amoled_18/pins_arduino.h
#define PIN_TOUCH_INT   21  // Touch INT (GPIO21)
#define TOUCH_I2C_ADDR  0x38

#define FT3168_REG_DEVICE_MODE  0x00
#define FT3168_REG_TD_STATUS    0x02
#define FT3168_REG_TOUCH1_XH    0x03
#define FT3168_REG_TOUCH1_XL    0x04
#define FT3168_REG_TOUCH1_YH    0x05
#define FT3168_REG_TOUCH1_YL    0x06

static bool touch_pressed = false;
static int16_t last_x = 0, last_y = 0;

esp_err_t hal_touch_init(void)
{
    ESP_LOGI(TAG, "Initializing FT3168 touch controller");

    // 配置中断引脚
    gpio_config_t int_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << PIN_TOUCH_INT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&int_conf);

    ESP_LOGI(TAG, "Touch controller initialized");
    return ESP_OK;
}

static esp_err_t ft3168_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    if (!hal_i2c_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    // 使用统一的 I2C 接口读取多个字节
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

bool hal_touch_read(int16_t *x, int16_t *y)
{
    uint8_t data[6];
    if (ft3168_read_reg(FT3168_REG_TD_STATUS, data, 6) != ESP_OK) {
        return false;
    }

    uint8_t touch_points = data[0] & 0x0F;
    if (touch_points == 0) {
        touch_pressed = false;
        return false;
    }

    // 解析坐标 (11-bit)
    int16_t raw_x = ((data[1] & 0x0F) << 8) | data[2];
    int16_t raw_y = ((data[3] & 0x0F) << 8) | data[4];

    // 坐标映射到屏幕 (368x448)
    *x = (raw_x * DISPLAY_WIDTH) / 2048;
    *y = (raw_y * DISPLAY_HEIGHT) / 2048;

    // 边界检查
    if (*x < 0) *x = 0;
    if (*x >= DISPLAY_WIDTH) *x = DISPLAY_WIDTH - 1;
    if (*y < 0) *y = 0;
    if (*y >= DISPLAY_HEIGHT) *y = DISPLAY_HEIGHT - 1;

    touch_pressed = true;
    last_x = *x;
    last_y = *y;

    return true;
}

bool hal_touch_get_event(touch_event_t *event)
{
    int16_t x, y;
    bool pressed = hal_touch_read(&x, &y);

    if (pressed && !touch_pressed) {
        event->type = TOUCH_EVT_PRESS;
        event->x = x;
        event->y = y;
        return true;
    } else if (!pressed && touch_pressed) {
        event->type = TOUCH_EVT_RELEASE;
        event->x = last_x;
        event->y = last_y;
        return true;
    } else if (pressed) {
        event->type = TOUCH_EVT_MOVE;
        event->x = x;
        event->y = y;
        return true;
    }

    event->type = TOUCH_EVT_NONE;
    return false;
}
