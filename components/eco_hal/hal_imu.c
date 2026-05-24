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
#define QMI8658_REG_CTRL8       0x09
#define QMI8658_REG_CTRL9       0x0A
#define QMI8658_REG_ACC_X_L     0x35

// CTRL1 位定义
#define CTRL1_ADDR_AI   (1 << 6)    // 地址自动递增（I2C burst 读取必需）
#define CTRL1_BE        (1 << 5)    // Big-Endian

static esp_err_t qmi8658_write_reg(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMU_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(IMU_I2C_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write FAILED: addr=0x%02X, reg=0x%02X, data=0x%02X, err=%s",
                 IMU_I2C_ADDR, reg, data, esp_err_to_name(ret));
    }
    return ret;
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

    // 打印 I2C 读取日志（调试用）
    // if (ret == ESP_OK) {
    //     ESP_LOGI(TAG, "I2C read OK: addr=0x%02X, reg=0x%02X, len=%u, data=%02X %02X %02X %02X %02X %02X",
    //              IMU_I2C_ADDR, reg, (unsigned)len,
    //              len > 0 ? data[0] : 0,
    //              len > 1 ? data[1] : 0,
    //              len > 2 ? data[2] : 0,
    //              len > 3 ? data[3] : 0,
    //              len > 4 ? data[4] : 0,
    //              len > 5 ? data[5] : 0);
    // } else {
    //     ESP_LOGE(TAG, "I2C read FAILED: addr=0x%02X, reg=0x%02X, len=%u, err=%s",
    //              IMU_I2C_ADDR, reg, (unsigned)len, esp_err_to_name(ret));
    // }
    return ret;
}

esp_err_t hal_imu_init(void)
{
    ESP_LOGI(TAG, "Initializing QMI8658 IMU (I2C addr=0x%02X)", IMU_I2C_ADDR);

    // 读取 WHO_AM_I 验证芯片存在
    uint8_t who_am_i = 0;
    esp_err_t ret = qmi8658_read_reg(QMI8658_REG_WHO_AM_I, &who_am_i, 1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "QMI8658 WHO_AM_I = 0x%02X (expect 0x05)", who_am_i);
    } else {
        ESP_LOGE(TAG, "QMI8658 WHO_AM_I read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 软件复位
    ESP_LOGI(TAG, "QMI8658 software reset...");
    qmi8658_write_reg(QMI8658_REG_CTRL1, 0x80);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 启用地址自动递增 (AAI)，否则 I2C burst 读取会返回重复字节
    // CTRL1: ADDR_AI=1, BE=1, 其余保持默认 0
    qmi8658_write_reg(QMI8658_REG_CTRL1, CTRL1_ADDR_AI | CTRL1_BE);
    ESP_LOGI(TAG, "QMI8658 CTRL1 set to 0x%02X (ADDR_AI=1, BE=1)", CTRL1_ADDR_AI | CTRL1_BE);

    // 配置加速度计: ±2g, 500Hz ODR
    // CTRL2: aODR=0x04 (500Hz), aFS=0x00 (±2g)
    qmi8658_write_reg(QMI8658_REG_CTRL2, 0x04);

    // 配置陀螺仪: ±2048dps, 500Hz ODR
    // CTRL3: gODR=0x04 (500Hz), gFS=0x00 (±2048dps)
    qmi8658_write_reg(QMI8658_REG_CTRL3, 0x04);

    // 使能加速度计和陀螺仪
    // CTRL7: 0x03 = acc_en | gyro_en
    qmi8658_write_reg(QMI8658_REG_CTRL7, 0x03);
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "QMI8658 initialized (acc+gyro enabled)");
    return ESP_OK;
}

// 摇晃冷却时间（ms），避免连续触发
#define SHAKE_COOLDOWN_MS 3000

shake_level_t hal_imu_detect_shake(void)
{
    static uint32_t last_shake_time = 0;
    uint32_t now = esp_timer_get_time() / 1000;

    // 冷却期内不检测
    if (now - last_shake_time < SHAKE_COOLDOWN_MS) {
        return SHAKE_NONE;
    }

    // burst 读取 6 个加速度/陀螺仪寄存器（AAI 已启用）
    uint8_t data[6];
    if (qmi8658_read_reg(QMI8658_REG_ACC_X_L, data, 6) != ESP_OK) {
        return SHAKE_NONE;
    }

    int16_t ax = (int16_t)(data[1] << 8 | data[0]);
    int16_t ay = (int16_t)(data[3] << 8 | data[2]);
    int16_t az = (int16_t)(data[5] << 8 | data[4]);

    // 避免开方和浮点运算，节省资源
    #define SHAKE_HEAVY_THRESHOLD 35000
    if (abs(ax) > SHAKE_HEAVY_THRESHOLD || abs(ay) > SHAKE_HEAVY_THRESHOLD || abs(az) > SHAKE_HEAVY_THRESHOLD) {
        last_shake_time = now;
        ESP_LOGI(TAG, "Shake detected: level=HEAVY, ax=%d, ay=%d, az=%d", ax, ay, az);
        return SHAKE_HEAVY;
    }

    float magnitude = sqrtf(ax * ax + ay * ay + az * az) / 16384.0f;
    float delta = fabsf(magnitude - 1.0f);

    shake_level_t level = SHAKE_NONE;
    if (delta > 1.3f)      level = SHAKE_HEAVY;
    else if (delta > 0.9f) level = SHAKE_MEDIUM;
    else if (delta > 0.5f) level = SHAKE_LIGHT;

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
