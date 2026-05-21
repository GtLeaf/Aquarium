#include "hal_pmu.h"
#include "hal_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_pmu";

#define AXP2101_ADDR        0x34

// AXP2101 register addresses
#define AXP2101_REG_PWRON_CFG     0x22
#define AXP2101_REG_PWRON_TIME    0x27
#define AXP2101_REG_DCDC_CFG      0x80
#define AXP2101_REG_LDO_CFG0      0x90
#define AXP2101_REG_LDO_CFG1      0x91
#define AXP2101_REG_DCDC1_VOLT    0x82
#define AXP2101_REG_ALDO1_VOLT    0x92
#define AXP2101_REG_CHG_CFG0      0x61
#define AXP2101_REG_CHG_CFG1      0x62
#define AXP2101_REG_CHG_CFG2      0x63
#define AXP2101_REG_CHG_VOLT      0x64

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
    ESP_LOGI(TAG, "Initializing AXP2101 PMU...");

    // Try both possible AXP2101 addresses (0x34 and 0x35)
    uint8_t chip_id = 0;
    esp_err_t ret;

    // Try 0x34 first
    ret = hal_i2c_read_byte(0x34, 0x00, &chip_id);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AXP2101 found at 0x34, chip ID: 0x%02X", chip_id);
    } else {
        // Try 0x35
        ret = hal_i2c_read_byte(0x35, 0x00, &chip_id);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "AXP2101 found at 0x35, chip ID: 0x%02X", chip_id);
        } else {
            ESP_LOGE(TAG, "Failed to find AXP2101 at 0x34 or 0x35");
            return ESP_FAIL;
        }
    }

    // AXP2101 initialization sequence for ESP32-S3-Touch-AMOLED-1.8
    // Based on xiaozhi-esp32 project

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

    // Set ALDO1 to 3.3V (formula: (3300 - 500) / 100 = 28)
    axp2101_write_byte(AXP2101_REG_ALDO1_VOLT, 28);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Enable ALDO1 (for MIC and other peripherals)
    axp2101_write_byte(AXP2101_REG_LDO_CFG0, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Charger configuration
    // CV charger voltage setting to 4.1V
    axp2101_write_byte(AXP2101_REG_CHG_VOLT, 0x02);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Set Main battery precharge current to 50mA
    axp2101_write_byte(AXP2101_REG_CHG_CFG0, 0x02);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Set Main battery charger current to 400mA
    axp2101_write_byte(AXP2101_REG_CHG_CFG1, 0x08);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Set Main battery term charge current to 25mA
    axp2101_write_byte(AXP2101_REG_CHG_CFG2, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "AXP2101 initialized successfully");

    // Read battery status
    uint8_t bat_percent = hal_pmu_get_battery_percent();
    ESP_LOGI(TAG, "Battery: %d%%", bat_percent);

    return ESP_OK;
}

uint8_t hal_pmu_get_battery_percent(void)
{
    // AXP2101 register 0xA4 contains battery percentage
    uint8_t percent = 0;
    esp_err_t ret = axp2101_read_byte(0xA4, &percent);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read battery percentage");
        return 0;
    }
    return percent;
}

bool hal_pmu_is_charging(void)
{
    // AXP2101 register 0x01 bit 5 indicates charging status
    uint8_t status = 0;
    esp_err_t ret = axp2101_read_byte(0x01, &status);
    if (ret != ESP_OK) {
        return false;
    }
    return (status & 0x20) != 0;
}
