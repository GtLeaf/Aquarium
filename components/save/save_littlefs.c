#include "save_littlefs.h"
#include "esp_log.h"
#include "esp_littlefs.h"

static const char *TAG = "save_littlefs";

esp_err_t save_littlefs_init(void)
{
    ESP_LOGI(TAG, "LittleFS init placeholder");
    return ESP_OK;
}

esp_err_t save_littlefs_write(const char *path, const void *data, size_t len)
{
    (void)path;
    (void)data;
    (void)len;
    return ESP_OK;
}

esp_err_t save_littlefs_read(const char *path, void *data, size_t len)
{
    (void)path;
    (void)data;
    (void)len;
    return ESP_OK;
}
