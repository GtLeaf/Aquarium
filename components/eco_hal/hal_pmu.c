#include "hal_pmu.h"
#include "hal_i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_pmu";

#define AXP2101_ADDR        0x34

#define AXP2101_REG_PWRON_CFG     0x22
#define AXP2101_REG_PWRON_TIME    0x27
#define AXP2101_REG_DCDC_CFG      0x80
#define AXP2101_REG_LDO_CFG0      0x90
#define AXP2101_REG_LDO_CFG1      0x91
#define AXP2101_REG_DCDC1_VOLT    0x82
#define AXP2101_REG_ALDO1_VOLT    0x92
#define AXP2101_REG_ALDO2_VOLT    0x93
#define AXP2101_REG_ALDO4_VOLT    0x95
#define AXP2101_REG_CHG_CFG0      0x61
#define AXP2101_REG_CHG_CFG1      0x62
#define AXP2101_REG_CHG_VOLT      0x64
#define AXP2101_REG_BAT_PERCENT   0xA4
#define AXP2101_REG_STATUS1       0x01

// 电量缓存（30秒刷新一次，避免频繁 I2C）
static uint8_t s_bat_cache = 0;
static bool    s_charging_cache = false;
static int64_t s_last_read_us = 0;
#define BAT_READ_INTERVAL_US  (30 * 1000000LL)

static esp_err_t axp2101_write_byte(uint8_t reg, uint8_t data)
{
    return hal_i2c_write_byte(AXP2101_ADDR, reg, data);
}

static esp_err_t axp2101_read_byte(uint8_t reg, uint8_t *data)
{
    return hal_i2c_read_byte(AXP2101_ADDR, reg, data);
}

esp_err_t hal_pmu_init(void)
{
    ESP_LOGI(TAG, "Initializing AXP2101 PMU");

    // 检测 AXP2101 是否存在
    uint8_t chip_id = 0;
    esp_err_t ret = axp2101_read_byte(0x00, &chip_id);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 not found at 0x%02X", AXP2101_ADDR);
        return ret;
    }
    ESP_LOGI(TAG, "AXP2101 found, chip ID: 0x%02X", chip_id);

    // PWRON > OFFLEVEL as POWEROFF Source enable
    axp2101_write_byte(AXP2101_REG_PWRON_CFG, 0b110);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Hold 4s to power off
    axp2101_write_byte(AXP2101_REG_PWRON_TIME, 0x10);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Disable All DCs but DC1
    axp2101_write_byte(AXP2101_REG_DCDC_CFG, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Disable All LDOs
    axp2101_write_byte(AXP2101_REG_LDO_CFG0, 0x00);
    axp2101_write_byte(AXP2101_REG_LDO_CFG1, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Set DC1 to 3.3V (formula: (3300 - 1500) / 100 = 18)
    axp2101_write_byte(AXP2101_REG_DCDC1_VOLT, 18);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Set ALDO1/ALDO2/ALDO4 to 3.3V (formula: (3300 - 500) / 100 = 28)
    axp2101_write_byte(AXP2101_REG_ALDO1_VOLT, 28);
    axp2101_write_byte(AXP2101_REG_ALDO2_VOLT, 28);
    axp2101_write_byte(AXP2101_REG_ALDO4_VOLT, 28);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Enable ALDO1 + ALDO2 + ALDO4
    // ALDO2 = 触摸供电, ALDO4 = 显示/TF卡供电
    axp2101_write_byte(AXP2101_REG_LDO_CFG0, 0x13);  // bit0=ALDO1, bit1=ALDO2, bit4=ALDO4
    vTaskDelay(pdMS_TO_TICKS(10));

    // Charger configuration
    axp2101_write_byte(AXP2101_REG_CHG_VOLT, 0x02);
    vTaskDelay(pdMS_TO_TICKS(10));
    axp2101_write_byte(AXP2101_REG_CHG_CFG0, 0x02);
    vTaskDelay(pdMS_TO_TICKS(10));
    axp2101_write_byte(AXP2101_REG_CHG_CFG1, 0x08);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Read battery status
    uint8_t bat_pct = 0;
    ret = axp2101_read_byte(0xA4, &bat_pct);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Battery: %d%%", bat_pct);
    }

    ESP_LOGI(TAG, "PMU initialized (ALDO1+ALDO2+ALDO4 enabled @ 3.3V)");
    return ESP_OK;
}

uint8_t hal_pmu_get_battery_percent(void)
{
    int64_t now = esp_timer_get_time();
    if (now - s_last_read_us > BAT_READ_INTERVAL_US || s_last_read_us == 0) {
        uint8_t raw = 0;
        esp_err_t ret = axp2101_read_byte(AXP2101_REG_BAT_PERCENT, &raw);
        if (ret == ESP_OK) {
            // AXP2101 0xA4 直接返回 0-100 百分比
            s_bat_cache = (raw > 100) ? 100 : raw;
        } else {
            ESP_LOGW(TAG, "Failed to read battery percent");
        }

        // 顺便读充电状态
        uint8_t status = 0;
        ret = axp2101_read_byte(AXP2101_REG_STATUS1, &status);
        if (ret == ESP_OK) {
            s_charging_cache = (status & 0x20) != 0; // bit5 = charging active
        }

        s_last_read_us = now;
    }
    return s_bat_cache;
}

bool hal_pmu_is_charging(void)
{
    // 确保缓存已刷新
    int64_t now = esp_timer_get_time();
    if (now - s_last_read_us > BAT_READ_INTERVAL_US || s_last_read_us == 0) {
        hal_pmu_get_battery_percent(); // 触发缓存刷新
    }
    return s_charging_cache;
}
