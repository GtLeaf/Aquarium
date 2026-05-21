#pragma once

#include "species_data.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ACTIVE_EVENTS 3

// 活跃事件实例
struct active_event {
    uint8_t event_id;
    uint32_t trigger_time;  // 触发时间戳
    uint32_t expiry_time;   // 过期时间戳（0=不过期）
    bool viewed;            // 用户是否已查看
};

// 事件触发器状态
struct event_trigger_state {
    struct active_event active[MAX_ACTIVE_EVENTS];
    uint8_t active_count;
    uint32_t last_trigger_time[24]; // 每种事件的最后触发时间
    uint8_t daily_trigger_count[24]; // 每种事件今日触发次数
    uint8_t total_today; // 今日总触发数
    uint16_t day_of_year; // 当前日期
};

// 初始化事件系统
void event_system_init(struct event_trigger_state *state);

// 每日重置（日期变化时调用）
void event_system_daily_reset(struct event_trigger_state *state, uint16_t day_of_year);

// 尝试触发事件（由引擎每秒调用）
void event_system_tick(struct event_trigger_state *state, struct game_save *save, uint32_t now);

// 检查是否有新事件待显示
bool event_system_has_pending(const struct event_trigger_state *state);

// 获取下一个待显示事件
const struct active_event* event_system_get_next_pending(struct event_trigger_state *state);

// 标记事件已查看
void event_system_mark_viewed(struct event_trigger_state *state, uint8_t event_id);

// 获取事件奖励（返回物种ID或0表示无生物奖励）
struct event_reward {
    uint32_t coins;
    uint8_t species_id; // 0=无生物奖励
    uint8_t nutrients;
};
struct event_reward event_system_get_reward(uint8_t event_id);

#ifdef __cplusplus
}
#endif