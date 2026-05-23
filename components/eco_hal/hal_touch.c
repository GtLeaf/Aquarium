#include "hal_touch.h"
#include "hal_i2c.h"
#include "hal_display.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "hal_touch";

#define PIN_TOUCH_INT   21
#define TOUCH_I2C_ADDR  0x38

#define PCA9554_ADDR    0x20
#define PCA9554_REG_OUT 0x01
#define PCA9554_REG_CFG 0x03

#define FT3168_REG_TD_STATUS    0x02
#define FT3168_REG_CHIP_ID      0xA3

static bool touch_pressed = false;
static int16_t last_x = 0, last_y = 0;

static esp_err_t ft3168_read_reg_byte(uint8_t reg, uint8_t *data)
{
    if (!hal_i2c_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static bool pca9554_deassert_touch_rst(void)
{
    uint8_t dummy = 0;
    esp_err_t ret = hal_i2c_read_byte(PCA9554_ADDR, 0x00, &dummy);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "PCA9554 not found, skip TP_RST");
        return false;
    }
    ESP_LOGI(TAG, "PCA9554 found, reading config...");
    
    uint8_t cfg = 0;
    hal_i2c_read_byte(PCA9554_ADDR, PCA9554_REG_CFG, &cfg);
    ESP_LOGI(TAG, "PCA9554 cfg before: 0x%02X", cfg);
    cfg &= ~0x04;  // pin 2 = output
    hal_i2c_write_byte(PCA9554_ADDR, PCA9554_REG_CFG, cfg);
    ESP_LOGI(TAG, "PCA9554 cfg after: 0x%02X", cfg);

    uint8_t out = 0;
    hal_i2c_read_byte(PCA9554_ADDR, PCA9554_REG_OUT, &out);
    ESP_LOGI(TAG, "PCA9554 out before: 0x%02X", out);
    out &= ~0x04;  // LOW reset
    hal_i2c_write_byte(PCA9554_ADDR, PCA9554_REG_OUT, out);
    ESP_LOGI(TAG, "TP_RST -> LOW (reset active)");
    vTaskDelay(pdMS_TO_TICKS(50));
    out |= 0x04;   // HIGH release
    hal_i2c_write_byte(PCA9554_ADDR, PCA9554_REG_OUT, out);
    ESP_LOGI(TAG, "TP_RST -> HIGH (reset released)");
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "TP_RST released (PCA9554 pin 2)");
    return true;
}

static esp_err_t ft3168_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    if (!hal_i2c_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < len; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg + i, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_READ, true);
        i2c_master_read_byte(cmd, &data[i], I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t hal_touch_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing FT3168 touch controller");
    ESP_LOGI(TAG, "INT pin = GPIO%d, I2C addr = 0x%02X", PIN_TOUCH_INT, TOUCH_I2C_ADDR);

    gpio_config_t int_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << PIN_TOUCH_INT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&int_conf);
    ESP_LOGI(TAG, "GPIO%d configured as input with pull-up", PIN_TOUCH_INT);

    // 先检查GPIO电平
    int level = gpio_get_level(PIN_TOUCH_INT);
    ESP_LOGI(TAG, "GPIO%d current level = %d (expect 1 when idle)", PIN_TOUCH_INT, level);

    pca9554_deassert_touch_rst();

    // 复位后等待芯片稳定（FT3168需要较长时间）
    ESP_LOGI(TAG, "Waiting 500ms for FT3168 to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(500));

    // 读取芯片ID验证通信
    uint8_t chip_id = 0;
    esp_err_t ret = ft3168_read_reg_byte(FT3168_REG_CHIP_ID, &chip_id);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FT3168 chip ID = 0x%02X (expect 0x64 for FT3168)", chip_id);
    } else {
        ESP_LOGE(TAG, "Failed to read chip ID: %s", esp_err_to_name(ret));
    }

    // 设置工作模式为 Normal Mode (0x00)
    // 某些批次需要显式设置，否则会停留在 factory/monitor 模式
    ESP_LOGI(TAG, "Setting FT3168 mode to NORMAL (0x00)...");
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, 0x00, true);  // MODE register
        i2c_master_write_byte(cmd, 0x00, true);  // Normal mode
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        ESP_LOGI(TAG, "Mode write result: %s", esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置中断触发方式为下降沿（低电平有效）
    // 某些FT3168需要配置 INT 引脚模式
    ESP_LOGI(TAG, "Configuring INT mode...");
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, 0xA4, true);  // INT mode register (varies by chip)
        i2c_master_write_byte(cmd, 0x00, true);  // 0x00 = polling mode, 0x01 = trigger mode
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        ESP_LOGI(TAG, "INT mode write result: %s", esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    // 尝试连续读取几个寄存器看通信是否正常
    uint8_t test_regs[8] = {0};
    ret = ft3168_read_reg(0x00, test_regs, 8);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FT3168 regs 0x00-0x07: %02X %02X %02X %02X %02X %02X %02X %02X",
                 test_regs[0], test_regs[1], test_regs[2], test_regs[3],
                 test_regs[4], test_regs[5], test_regs[6], test_regs[7]);
    } else {
        ESP_LOGE(TAG, "Failed to read test regs: %s", esp_err_to_name(ret));
    }

    // 读取一些状态寄存器
    uint8_t reg_a4 = 0, reg_a5 = 0, reg_a6 = 0;
    ft3168_read_reg_byte(0xA4, &reg_a4);
    ft3168_read_reg_byte(0xA5, &reg_a5);
    ft3168_read_reg_byte(0xA6, &reg_a6);
    ESP_LOGI(TAG, "FT3168 reg 0xA4=0x%02X, 0xA5=0x%02X, 0xA6=0x%02X", reg_a4, reg_a5, reg_a6);

    // 读取固件版本号 (0xA6-0xAF)
    uint8_t fw_ver[10] = {0};
    ret = ft3168_read_reg(0xA6, fw_ver, 10);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FT3168 FW ver 0xA6-0xAF: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3], fw_ver[4],
                 fw_ver[5], fw_ver[6], fw_ver[7], fw_ver[8], fw_ver[9]);
    }

    // 读取LIB版本 (0xA1-0xA2)
    uint8_t lib_ver[2] = {0};
    ret = ft3168_read_reg(0xA1, lib_ver, 2);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FT3168 LIB ver 0xA1-0xA2: %02X %02X", lib_ver[0], lib_ver[1]);
    }

    // 读取状态寄存器 0x80
    uint8_t reg_80 = 0;
    ft3168_read_reg_byte(0x80, &reg_80);
    ESP_LOGI(TAG, "FT3168 reg 0x80=0x%02X", reg_80);

    ESP_LOGI(TAG, "Touch controller initialized");
    ESP_LOGI(TAG, "========================================");
    return ESP_OK;
}

// 尝试burst读取
static esp_err_t ft3168_read_burst(uint8_t reg, uint8_t *data, size_t len)
{
    if (!hal_i2c_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TOUCH_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

bool hal_touch_read(int16_t *x, int16_t *y)
{
    static int read_count = 0;
    static int last_log_count = 0;
    read_count++;

    // 每10次读取打印一次调试信息（约每秒3次）
    bool should_log = (read_count - last_log_count >= 10);
    if (should_log) {
        last_log_count = read_count;
    }

    uint8_t data[6];
    // 先尝试burst读取
    esp_err_t ret = ft3168_read_burst(FT3168_REG_TD_STATUS, data, 6);
    if (ret != ESP_OK) {
        // burst失败，回退到byte-by-byte
        ret = ft3168_read_reg(FT3168_REG_TD_STATUS, data, 6);
    }
    if (ret != ESP_OK) {
        if (should_log) {
            ESP_LOGW(TAG, "[#%d] I2C FAILED: %s", read_count, esp_err_to_name(ret));
        }
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

    // START button area: x=114~254, y=320~368 (based on ui_screens.c)
    // Also check RESET button: x=20~80, y=410~438
    bool in_start_btn = (raw_x >= 114 && raw_x <= 254 && raw_y >= 320 && raw_y <= 368);
    bool in_reset_btn = (raw_x >= 20 && raw_x <= 80 && raw_y >= 410 && raw_y <= 438);
    const char *btn_hint = in_start_btn ? " [START]" : (in_reset_btn ? " [RESET]" : "");

    // ESP_LOGI(TAG, "[#%d] TOUCH! pts=%d raw=(%d,%d)%s", read_count, touch_points, raw_x, raw_y, btn_hint);

    // 该批次 FT3168 直接输出接近屏幕分辨率的坐标，无需再映射
    *x = raw_x;
    *y = raw_y;

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
