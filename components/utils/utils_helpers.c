#include "utils_helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t utils_get_tick_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void utils_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
