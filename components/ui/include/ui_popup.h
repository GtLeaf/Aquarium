#pragma once

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 弹窗类型
enum popup_type {
    POPUP_NONE = 0,
    POPUP_EVENT,       // 事件弹窗
    POPUP_OFFLINE,     // 离线收益弹窗
    POPUP_REWARD,      // 通用奖励弹窗
    POPUP_ACHIEVEMENT, // 成就解锁弹窗
};

// 显示事件弹窗
void ui_popup_show_event(uint8_t event_id, const char *title, const char *desc);

// 显示离线收益弹窗
void ui_popup_show_offline(uint32_t coins, uint32_t hours, uint8_t creatures);

// 显示通用奖励弹窗
void ui_popup_show_reward(const char *title, const char *msg);

// 关闭当前弹窗
void ui_popup_close(void);

// 是否有弹窗显示中
bool ui_popup_is_visible(void);

// 弹窗点击回调（由触摸系统调用）
void ui_popup_on_click(void);

// 显示成就解锁弹窗
void ui_popup_show_achievement(const char *name, const char *desc);

#ifdef __cplusplus
}
#endif
