#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 音效类型
enum sound_effect {
    SOUND_NONE = 0,
    SOUND_CLICK,       // 点击
    SOUND_FEED,        // 喂食
    SOUND_EVENT,       // 事件发生
    SOUND_REWARD,      // 获得奖励
    SOUND_UNLOCK,      // 解锁新物种
    SOUND_WARNING,     // 警告（低氧/饥饿）
};

esp_err_t hal_audio_init(void);
void hal_audio_play(enum sound_effect effect);
void hal_audio_set_mute(bool mute);
bool hal_audio_is_muted(void);

#ifdef __cplusplus
}
#endif