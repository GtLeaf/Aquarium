#include "hal_imu.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include <math.h>

static const char *TAG = "hal_imu";

// QMI8658 IMU 引脚定义 (与触摸共用 I2C 总线)
#define IMU_I2C_ADDR    0x6B
#define IMU_I2C_NUM     I2C_NUM_0

#define QMI8658_REG_WHO_AM_I    0x00
#define QMI8658_REG_CTRL1       0x02
#define QMI8658_REG_CTRL2       0x03
#define QMI8658_REG_CTRL3       0x04
#define QMI8658_REG_CTRL5       0x06
#define QMI8658_REG_CTRL7       0x08
#define QMI8658_REG_ACC_X_L     0x35

esp_err_t hal_imu_init(void)
{
    ESP_LOGI(TAG, "Initializing QMI8658 IMU");
    // 实际硬件初始化代码
    // 配置加速度计和陀螺仪
    return ESP_OK;
}

static esp_err_t qmi8658_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMU_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMU_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(IMU_I2C_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// 摇晃冷却时间（ms），避免连续触发
#define SHAKE_COOLDOWN_MS 5000

shake_level_t hal_imu_detect_shake(void)
{
    static uint32_t last_shake_time = 0;
    uint32_t now = esp_timer_get_time() / 1000;

    // 冷却期内不检测
    if (now - last_shake_time < SHAKE_COOLDOWN_MS) {
        return SHAKE_NONE;
    }

    uint8_t data[6];
    if (qmi8658_read_reg(QMI8658_REG_ACC_X_L, data, 6) != ESP_OK) {
        return SHAKE_NONE;
    }

    int16_t ax = (int16_t)(data[1] << 8 | data[0]);
    int16_t ay = (int16_t)(data[3] << 8 | data[2]);
    int16_t az = (int16_t)(data[5] << 8 | data[4]);

    float magnitude = sqrtf(ax * ax + ay * ay + az * az) / 16384.0f;
    float delta = fabsf(magnitude - 1.0f);

    shake_level_t level = SHAKE_NONE;
    if (delta > 1.0f)      level = SHAKE_HEAVY;
    else if (delta > 0.8f) level = SHAKE_MEDIUM;
    else if (delta > 0.3f) level = SHAKE_LIGHT;

    if (level != SHAKE_NONE) {
        last_shake_time = now;
        ESP_LOGI(TAG, "Shake detected: level=%d, delta=%.2f, ax=%d, ay=%d, az=%d", level, delta, ax, ay, az);
    }
    return level;
}

void hal_imu_get_tilt(float *pitch, float *roll)
{
    uint8_t data[6];
    if (qmi8658_read_reg(QMI8658_REG_ACC_X_L, data, 6) != ESP_OK) {
        *pitch = 0;
        *roll = 0;
        return;
    }

    float ax = (int16_t)(data[1] << 8 | data[0]) / 16384.0f;
    float ay = (int16_t)(data[3] << 8 | data[2]) / 16384.0f;
    float az = (int16_t)(data[5] << 8 | data[4]) / 16384.0f;

    *pitch = atan2f(ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    *roll = atan2f(ay, sqrtf(ax * ax + az * az)) * 180.0f / M_PI;
}

enum shake_effect hal_imu_get_shake_effect(shake_level_t level)
{
    switch (level) {
        case SHAKE_LIGHT:  return SHAKE_EFFECT_FEED;
        case SHAKE_MEDIUM: return SHAKE_EFFECT_OXYGEN;
        case SHAKE_HEAVY:  return SHAKE_EFFECT_SCATTER;
        default: return SHAKE_EFFECT_NONE;
    }
}

bool hal_imu_detect_water_cycle(float pitch, float roll)
{
    // 90度翻转检测：pitch 或 roll 接近 90 度
    return (fabsf(pitch) > 75.0f || fabsf(roll) > 75.0f);
}
