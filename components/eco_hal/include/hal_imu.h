#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHAKE_NONE = 0,
    SHAKE_LIGHT,
    SHAKE_MEDIUM,
    SHAKE_HEAVY,
} shake_level_t;

// 摇晃效果类型
enum shake_effect {
    SHAKE_EFFECT_NONE = 0,
    SHAKE_EFFECT_SCATTER,   // 驱散：L1 生物 50% 死亡
    SHAKE_EFFECT_FEED,      // 喂食：所有生物饥饿 -30
    SHAKE_EFFECT_OXYGEN,    // 增氧：氧气 +20
    SHAKE_EFFECT_EVENT,     // 触发事件
};

esp_err_t hal_imu_init(void);
shake_level_t hal_imu_detect_shake(void);
void hal_imu_get_tilt(float *pitch, float *roll);

// 获取摇晃效果（由引擎调用）
enum shake_effect hal_imu_get_shake_effect(shake_level_t level);

// 倾斜效果：90度翻转触发水循环（加速藻类生长）
bool hal_imu_detect_water_cycle(float pitch, float roll);

#ifdef __cplusplus
}
#endif
