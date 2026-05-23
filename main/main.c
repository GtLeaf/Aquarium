#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal_i2c.h"
#include "hal_display.h"
#include "hal_touch.h"
#include "hal_lvgl.h"
#include "hal_audio.h"
#include "hal_pmu.h"
#include "ui_main.h"
#include "engine_main.h"
#include "save_manager.h"
#include "utils_helpers.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-S3 Touch AMOLED 1.8 starting...");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "========================================");

    // 1. 初始化底层存储和硬件
    ESP_LOGI(TAG, "[1/9] Initializing save manager...");
    ESP_ERROR_CHECK(save_manager_init());
    ESP_LOGI(TAG, "Save manager OK");

    // 初始化 I2C 总线（PMU 和触摸共用）
    ESP_LOGI(TAG, "[2/9] Initializing I2C bus...");
    ESP_ERROR_CHECK(hal_i2c_init());
    ESP_LOGI(TAG, "I2C OK");

    // 初始化 PMU (AXP2101) - 必须先于显示初始化
    ESP_LOGI(TAG, "[3/9] Initializing PMU...");
    esp_err_t pmu_ret = hal_pmu_init();
    if (pmu_ret != ESP_OK) {
        ESP_LOGW(TAG, "PMU init failed: %s, continuing without PMU", esp_err_to_name(pmu_ret));
    } else {
        ESP_LOGI(TAG, "PMU OK");
    }

    ESP_LOGI(TAG, "[4/9] Initializing display...");
    ESP_ERROR_CHECK(hal_display_init());
    ESP_LOGI(TAG, "Display OK");

    ESP_LOGI(TAG, "[5/9] Initializing touch...");
    esp_err_t touch_ret = hal_touch_init();
    if (touch_ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed: %s, continuing without touch", esp_err_to_name(touch_ret));
    } else {
        ESP_LOGI(TAG, "Touch OK");
    }

    ESP_LOGI(TAG, "[6/9] Initializing audio...");
    ESP_ERROR_CHECK(hal_audio_init());
    ESP_LOGI(TAG, "Audio OK");

    ESP_LOGI(TAG, "[7/9] Initializing LVGL...");
    ESP_ERROR_CHECK(hal_lvgl_init());
    ESP_LOGI(TAG, "LVGL OK");

    // 2. 初始化游戏引擎（加载存档等）
    ESP_LOGI(TAG, "[8/9] Initializing game engine...");
    ESP_ERROR_CHECK(engine_init());
    ESP_LOGI(TAG, "Engine OK");

    // 3. 初始化 UI（创建 LVGL 对象树，此时还没有 LVGL 渲染任务运行，线程安全）
    ESP_LOGI(TAG, "[9/9] Initializing UI...");
    ESP_ERROR_CHECK(ui_init());
    ESP_LOGI(TAG, "UI OK");

    // 4. UI 初始化完成后再启动 LVGL 渲染任务（避免竞态）
    ESP_LOGI(TAG, "Starting LVGL task...");
    xTaskCreatePinnedToCore(hal_lvgl_port_task, "lvgl_task", 8192, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "LVGL task started");

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "All components initialized successfully!");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "========================================");

    while (1) {
        engine_tick();
        ui_update();
        vTaskDelay(pdMS_TO_TICKS(32));
    }
}
