#include "hal_lvgl.h"
#include "hal_display.h"
#include "hal_touch.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_lvgl";

static lv_display_t *disp = NULL;
static lv_indev_t *indev = NULL;
static uint8_t *buf1 = NULL;
static uint8_t *buf2 = NULL;

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    hal_display_flush_cb(display, area, px_map);
}

static void lvgl_touch_cb(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;
    static int call_count = 0;
    static int last_log_call = 0;
    static bool was_pressed = false;
    static int16_t last_x = 0, last_y = 0;
    call_count++;

    int16_t x, y;
    bool pressed = hal_touch_read(&x, &y);

    // 每10次调用打印一次，确认回调被LVGL调用
    if (call_count - last_log_call >= 10) {
        last_log_call = call_count;
        ESP_LOGI(TAG, "[cb#%d] pressed=%d x=%d y=%d", call_count, pressed ? 1 : 0, x, y);
    }

    if (pressed) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        if (!was_pressed) {
            bool in_start = (x >= 114 && x <= 254 && y >= 320 && y <= 368);
            ESP_LOGI(TAG, "【按下】cb#%d x=%d,y=%d START=%s", call_count, x, y, in_start ? "Y" : "N");
        } else if (x != last_x || y != last_y) {
            ESP_LOGI(TAG, "【移动】cb#%d x=%d,y=%d", call_count, x, y);
        }
        last_x = x;
        last_y = y;
        was_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        if (was_pressed) {
            ESP_LOGI(TAG, "【抬起】cb#%d (%d,%d)", call_count, last_x, last_y);
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

    ESP_LOGI(TAG, "LVGL initialized successfully");
    ESP_LOGI(TAG, "========================================");
    return ESP_OK;
}

void hal_lvgl_port_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "LVGL task started");

    int loop_count = 0;
    while (1) {
        uint32_t task_delay_ms = lv_timer_handler();
        if (task_delay_ms > 500) {
            task_delay_ms = 500;
        } else if (task_delay_ms < 5) {
            task_delay_ms = 5;
        }

        loop_count++;
        if (loop_count % 100 == 0) {
            // ESP_LOGI(TAG, "LVGL task running (loop=%d, delay=%lu)", loop_count, task_delay_ms);
        }

        // 独立轮询触摸（绕过LVGL输入设备，用于调试）
        static int poll_count = 0;
        static int last_poll_log = 0;
        poll_count++;
        int16_t tx, ty;
        bool tpressed = hal_touch_read(&tx, &ty);
        if (tpressed || (poll_count - last_poll_log >= 50)) {
            last_poll_log = poll_count;
            // ESP_LOGI(TAG, "[POLL#%d] touch=%d x=%d y=%d", poll_count, tpressed ? 1 : 0, tx, ty);
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}
