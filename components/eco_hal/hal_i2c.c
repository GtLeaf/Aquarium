#include "hal_i2c.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char *TAG = "hal_i2c";

#define I2C_PORT        I2C_NUM_0
#define I2C_SDA_PIN     14
#define I2C_SCL_PIN     15
#define I2C_FREQ_HZ     400000

static bool s_i2c_initialized = false;

esp_err_t hal_i2c_init(void)
{
    if (s_i2c_initialized) {
        ESP_LOGW(TAG, "I2C already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2C (SDA=%d, SCL=%d, freq=%dHz)", I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_i2c_initialized = true;
    ESP_LOGI(TAG, "I2C initialized OK");
    return ESP_OK;
}

esp_err_t hal_i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *data)
{
    if (!s_i2c_initialized) return ESP_ERR_INVALID_STATE;
    if (!data) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t hal_i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t data)
{
    if (!s_i2c_initialized) return ESP_ERR_INVALID_STATE;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

bool hal_i2c_is_initialized(void)
{
    return s_i2c_initialized;
}
