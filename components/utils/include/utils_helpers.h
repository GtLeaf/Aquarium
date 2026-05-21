#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t utils_get_tick_ms(void);
void utils_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
