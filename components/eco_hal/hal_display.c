#include "hal_display.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "esp_lcd_sh8601.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_display";

static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

// AMOLED 1.8 QSPI 引脚定义 (ESP32-S3-Touch-AMOLED-1.8)
// 根据 Arduino 官方引脚定义: https://github.com/espressif/arduino-esp32/blob/3.3.1/variants/waveshare_esp32_s3_touch_amoled_18/pins_arduino.h
// SH8601 uses QSPI (4-wire SPI)
#define PIN_LCD_CS      12  // QSPI CS
#define PIN_LCD_RST     17  // Reset (GPIO17)
#define PIN_LCD_PCLK    11  // QSPI CLK (GPIO11)
#define PIN_LCD_DATA0   4   // QSPI D0
#define PIN_LCD_DATA1   5   // QSPI D1
#define PIN_LCD_DATA2   6   // QSPI D2
#define PIN_LCD_DATA3   7   // QSPI D3

// SH8601 backlight is controlled via command 0x51
#define PIN_LCD_BL      (-1)  // No GPIO backlight control

// QSPI 写命令操作码（与 esp_lcd_sh8601.c 中的 LCD_OPCODE_WRITE_CMD 一致）
#define LCD_OPCODE_WRITE_CMD    (0x02ULL)
#define SH8601_CMD_BRIGHTNESS   (0x51)

// SH8601 custom initialization commands for proper color format
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    // {cmd, { data }, data_size, delay_ms}
    {0x11, (uint8_t []){0x00}, 0, 120},  // Sleep out
    {0x44, (uint8_t []){0x00, 0xc8}, 2, 0},  // Set tear scanline
    {0x35, (uint8_t []){0x00}, 1, 0},  // Set tear on
    {0x53, (uint8_t []){0x20}, 1, 25},  // Write CTRL display
    {0x3A, (uint8_t []){0x05}, 1, 0},  // Color mode: 16-bit RGB565
    {0x29, (uint8_t []){0x00}, 0, 120},  // Display on
};

esp_err_t hal_display_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing AMOLED SH8601 display (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    ESP_LOGI(TAG, "QSPI Pins: CS=%d RST=%d PCLK=%d D0=%d D1=%d D2=%d D3=%d",
             PIN_LCD_CS, PIN_LCD_RST, PIN_LCD_PCLK, PIN_LCD_DATA0, PIN_LCD_DATA1, PIN_LCD_DATA2, PIN_LCD_DATA3);

    // 配置背光/电源使能 GPIO (GPIO38)
    // GPIO38 is QSPI CLK, no separate power enable needed
    // AXP2101 handles AMOLED power
    ESP_LOGI(TAG, "Step 1: Skipping GPIO power enable (GPIO38 is QSPI CLK)");

    // 配置 QSPI 总线
    ESP_LOGI(TAG, "Step 2: Initializing QSPI bus (host=%d)", SPI2_HOST);
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_LCD_PCLK,
        .mosi_io_num = PIN_LCD_DATA0,
        .miso_io_num = PIN_LCD_DATA1,  // QSPI uses D1 as MISO
        .quadwp_io_num = PIN_LCD_DATA2,
        .quadhd_io_num = PIN_LCD_DATA3,
        .max_transfer_sz = DISPLAY_WIDTH * 2 * 80, // 80 lines per transfer
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QSPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "QSPI bus initialized OK");

    // 配置 LCD 面板 IO (QSPI)
    ESP_LOGI(TAG, "Step 3: Creating panel IO (cs=%d, pclk=%dMHz)", PIN_LCD_CS, 20);
    esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(PIN_LCD_CS, NULL, NULL);
    io_config.pclk_hz = 20 * 1000 * 1000; // 20MHz, lower to avoid DMA underflow
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel IO creation failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Panel IO created OK");

    // 配置 SH8601 面板
    ESP_LOGI(TAG, "Step 4: Creating SH8601 panel driver...");
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(sh8601_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ret = esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SH8601 panel creation failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SH8601 panel created OK");

    ESP_LOGI(TAG, "Step 5: Resetting panel");
    ret = esp_lcd_panel_reset(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel reset failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Panel reset OK");

    ESP_LOGI(TAG, "Step 6: Initializing panel");
    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Panel init OK");

    ESP_LOGI(TAG, "Step 7: Setting orientation");
    // SH8601 does not support swap_xy
    esp_lcd_panel_mirror(panel_handle, false, false);
    esp_lcd_panel_set_gap(panel_handle, 0, 0);
    ESP_LOGI(TAG, "Orientation set OK");

    ESP_LOGI(TAG, "Step 8: Turning display ON");
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display on failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Display ON OK");

    // 设置亮度 (SH8601: command 0x51)
    ESP_LOGI(TAG, "Step 9: Setting brightness to max");
    uint8_t brightness = 0xFF;
    esp_lcd_panel_io_tx_param(io_handle, 0x51, &brightness, 1);
    ESP_LOGI(TAG, "Brightness set OK");

    // 清屏为测试条纹（调试用，验证颜色通道）
    ESP_LOGI(TAG, "Step 10: Clearing screen to test pattern (R/G/B stripes)");
    size_t clear_buf_size = DISPLAY_WIDTH * 2 * 40; // 40 lines at once
    uint8_t *clear_buf = heap_caps_malloc(clear_buf_size, MALLOC_CAP_DMA);
    if (clear_buf) {
        for (int y = 0; y < DISPLAY_HEIGHT; y += 40) {
            int h = (y + 40 > DISPLAY_HEIGHT) ? (DISPLAY_HEIGHT - y) : 40;
            // Test pattern: Red/Green/Blue vertical stripes
            for (int row = 0; row < h; row++) {
                for (int x = 0; x < DISPLAY_WIDTH; x++) {
                    int idx = (row * DISPLAY_WIDTH + x) * 2;
                    if (x < DISPLAY_WIDTH / 3) {
                        // Red: RGB565 = 0xF800, little endian: [0x00, 0xF8]
                        clear_buf[idx] = 0x00;
                        clear_buf[idx + 1] = 0xF8;
                    } else if (x < DISPLAY_WIDTH * 2 / 3) {
                        // Green: RGB565 = 0x07E0, little endian: [0xE0, 0x07]
                        clear_buf[idx] = 0xE0;
                        clear_buf[idx + 1] = 0x07;
                    } else {
                        // Blue: RGB565 = 0x001F, little endian: [0x1F, 0x00]
                        clear_buf[idx] = 0x1F;
                        clear_buf[idx + 1] = 0x00;
                    }
                }
            }
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, DISPLAY_WIDTH, y + h, clear_buf);
            vTaskDelay(pdMS_TO_TICKS(1)); // yield to avoid watchdog
        }
        free(clear_buf);
        ESP_LOGI(TAG, "Screen cleared to test pattern");
    } else {
        ESP_LOGE(TAG, "Failed to allocate clear buffer!");
    }

    ESP_LOGI(TAG, "Display initialized successfully");
    ESP_LOGI(TAG, "========================================");
    return ESP_OK;
}

// 亮度调节延迟写入（避免与 LVGL flush 冲突）
static volatile uint8_t s_pending_brightness = 0;
static volatile bool s_brightness_pending = false;

// 在 flush_cb 中执行实际的亮度写入（此时 QSPI 总线安全）
static void hal_display_apply_brightness(void)
{
    if (s_brightness_pending && io_handle) {
        s_brightness_pending = false;
        uint8_t brightness = s_pending_brightness;
        int lcd_cmd = (SH8601_CMD_BRIGHTNESS << 8) | (LCD_OPCODE_WRITE_CMD << 24);
        esp_err_t ret = esp_lcd_panel_io_tx_param(io_handle, lcd_cmd, &brightness, 1);
        ESP_LOGI(TAG, "Brightness applied: %d, ret=%s", brightness, esp_err_to_name(ret));
    }
}

void hal_display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    // 在刷新完成后应用待处理的亮度调节（避免 QSPI 总线冲突）
    hal_display_apply_brightness();

    lv_display_flush_ready(disp);
}

void hal_display_set_brightness(uint8_t brightness)
{
    // 不在此处直接发送 QSPI 命令，避免与 LVGL flush 冲突
    // 改为标记待写入，由 hal_display_flush_cb 在刷新完成后执行
    s_pending_brightness = brightness;
    s_brightness_pending = true;
    ESP_LOGI(TAG, "Brightness queued: %d", brightness);
}
