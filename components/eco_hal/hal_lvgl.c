#include "hal_lvgl.h"
#include "hal_display.h"
#include "hal_touch.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_lvgl";

static lv_display_t *disp = NULL;
static lv_indev_t *indev = NULL;
static uint8_t *buf1 = NULL;
static uint8_t *buf2 = NULL;
static void (*s_ui_update_cb)(void) = NULL;
static void (*s_ui_interaction_cb)(void) = NULL;

void hal_lvgl_set_ui_update_cb(void (*cb)(void))
{
    s_ui_update_cb = cb;
}

void hal_lvgl_set_interaction_cb(void (*cb)(void))
{
    s_ui_interaction_cb = cb;
}

static uint32_t lvgl_tick_get_cb(void)
{
   return (uint32_t)(esp_timer_get_time() / 1000);
}

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    hal_display_flush_cb(display, area, px_map);
}

static void lvgl_touch_cb(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;
    static int call_count = 0;
    static bool was_pressed = false;
    static int16_t last_x = 0, last_y = 0;
    call_count++;

    int16_t x, y;
    bool pressed = hal_touch_read(&x, &y);

    if (pressed) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        if (!was_pressed) {
            if (s_ui_interaction_cb) s_ui_interaction_cb();
            bool in_start = (x >= 114 && x <= 254 && y >= 320 && y <= 368);
            ESP_LOGI(TAG, "[LVGL Press] cb#%d x=%d,y=%d START=%s", call_count, x, y, in_start ? "Y" : "N");
        }
        last_x = x;
        last_y = y;
        was_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        if (was_pressed) {
            ESP_LOGI(TAG, "[LVGL Release] cb#%d (%d,%d)", call_count, last_x, last_y);
        }
        was_pressed = false;
    }
}

// SH8601 requires coordinates to be aligned to 2 pixels
static void lvgl_rounder_cb(lv_event_t *e)
{
    lv_area_t *area = lv_event_get_param(e);
    if (area) {
        area->x1 = (area->x1 >> 1) << 1;
        area->y1 = (area->y1 >> 1) << 1;
        area->x2 = ((area->x2 >> 1) << 1) + 1;
        area->y2 = ((area->y2 >> 1) << 1) + 1;
    }
}

esp_err_t hal_lvgl_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing LVGL");

    ESP_LOGI(TAG, "Calling lv_init()...");
    lv_init();
    ESP_LOGI(TAG, "lv_init() OK");

    // 注册tick 回调
    lv_tick_set_cb(lvgl_tick_get_cb);
    ESP_LOGI(TAG, "Tick callback registered OK");

    // 分配显示缓冲 (使用 PSRAM)
    // LVGL 9.x: buf_size 参数是字节数
    size_t buf_pixels = DISPLAY_WIDTH * 40; // 40 lines
    size_t buf_bytes = buf_pixels * sizeof(lv_color_t);
    ESP_LOGI(TAG, "Buffer config: %dx%d pixels, %d lines, %u bytes per buffer",
             DISPLAY_WIDTH, DISPLAY_HEIGHT, 40, (unsigned)buf_bytes);

    ESP_LOGI(TAG, "Allocating buffer 1 (%u bytes from PSRAM)...", (unsigned)buf_bytes);
    buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
    if (!buf1) {
        ESP_LOGW(TAG, "PSRAM alloc failed, trying internal RAM");
        buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL);
    }
    if (!buf1) {
        ESP_LOGE(TAG, "Failed to allocate buffer 1");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Buffer 1 allocated at %p", (void*)buf1);

    ESP_LOGI(TAG, "Allocating buffer 2 (%u bytes from PSRAM)...", (unsigned)buf_bytes);
    buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
    if (!buf2) {
        ESP_LOGW(TAG, "PSRAM alloc failed, trying internal RAM");
        buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL);
    }
    if (!buf2) {
        ESP_LOGE(TAG, "Failed to allocate buffer 2");
        free(buf1);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Buffer 2 allocated at %p", (void*)buf2);

    // LVGL 9.x display创建
    ESP_LOGI(TAG, "Creating LVGL display %dx%d...", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to create display");
        free(buf1); free(buf2);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Display created OK");

    ESP_LOGI(TAG, "Setting color format to RGB565_SWAPPED...");
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);

    ESP_LOGI(TAG, "Setting flush callback...");
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    ESP_LOGI(TAG, "Setting buffers (mode=PARTIAL, size=%u)...", (unsigned)buf_bytes);
    lv_display_set_buffers(disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 初始化触摸输入设备
    ESP_LOGI(TAG, "Creating input device...");
    indev = lv_indev_create();
    if (!indev) {
        ESP_LOGE(TAG, "Failed to create input device");
        return ESP_ERR_NO_MEM;
    }
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
    lv_indev_set_display(indev, disp);
    lv_indev_enable(indev, true);
    ESP_LOGI(TAG, "Input device created OK (indev=%p)", (void*)indev);

    // LVGL 9.x: set input device mode to TIMER (poll-based)
    lv_indev_set_mode(indev, LV_INDEV_MODE_TIMER);
    ESP_LOGI(TAG, "Input device mode set to TIMER");

    // LVGL 9.x: create default group and assign indev to it
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(indev, g);
    ESP_LOGI(TAG, "Input device assigned to default group");

    // Check input device timer status
    lv_timer_t *read_timer = lv_indev_get_read_timer(indev);
    if(read_timer) {
        bool paused = lv_timer_get_paused(read_timer);
        ESP_LOGI(TAG, "Input device read_timer: %p, paused=%s", (void*)read_timer, paused ? "YES" : "NO");
    } else {
        ESP_LOGW(TAG, "Input device read_timer is NULL!");
    }

    ESP_LOGI(TAG, "LVGL initialized successfully");
    ESP_LOGI(TAG, "========================================");
    return ESP_OK;
}

void hal_lvgl_port_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "LVGL task started");

    esp_task_wdt_add(NULL);

        int loop_count = 0;
    while (1) {
        
        esp_task_wdt_reset();

        // UI 逻辑更新（与 LVGL 同线程，保证线程安全）
        if (s_ui_update_cb) s_ui_update_cb();

        uint32_t task_delay_ms = lv_timer_handler();
        if (task_delay_ms > 500) {
            task_delay_ms = 500;
        } else if (task_delay_ms < 10) {
            task_delay_ms = 10;
        }

        loop_count++;
        if (loop_count % 200 == 0) {
            ESP_LOGI(TAG, "LVGL task alive (loop=%d)", loop_count);
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}
