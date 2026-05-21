#include "hal_rtc.h"
#include "esp_log.h"
#include <sys/time.h>

static const char *TAG = "hal_rtc";

esp_err_t hal_rtc_init(void)
{
    ESP_LOGI(TAG, "RTC initialized (using system time)");
    return ESP_OK;
}

struct tm hal_rtc_get_time(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

void hal_rtc_set_time(const struct tm *timeinfo)
{
    struct timeval tv = {
        .tv_sec = mktime((struct tm *)timeinfo),
        .tv_usec = 0
    };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "RTC time set");
}

bool hal_rtc_is_daytime(void)
{
    struct tm t = hal_rtc_get_time();
    int hour = t.tm_hour;
    return (hour >= 6 && hour < 18);
}
