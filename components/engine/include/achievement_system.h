#pragma once

#include "species_data.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 成就ID
#define ACHV_FIRST_BREED    0   // 首次繁殖
#define ACHV_COLLECT_10     1   // 收集10种物种
#define ACHV_COLLECT_ALL    2   // 收集全部30种
#define ACHV_TANK_MAX       3   // 缸升到满级
#define ACHV_COINS_1000     4   // 拥有1000光合币
#define ACHV_SURVIVE_7D     5   // 存活7天
#define ACHV_GIANT_CREATURE 6   // 培养出巨型生物
#define ACHV_EVENT_50       7   // 触发50次事件
#define ACHV_COUNT          8

struct achievement_state {
    uint32_t breed_count;
    uint32_t event_triggered;
    uint32_t max_coins_held;
    uint32_t days_survived;
    uint64_t achievements_unlocked; // 位图
};

// 成就解锁回调（由UI层设置）
typedef void (*achievement_unlocked_cb_t)(uint8_t achv_id, const char *name, const char *desc);

void achievement_init(struct achievement_state *state);
void achievement_check(struct achievement_state *state, const struct game_save *save);
bool achievement_is_unlocked(const struct achievement_state *state, uint8_t achv_id);
const char* achievement_get_name(uint8_t achv_id);
const char* achievement_get_desc(uint8_t achv_id);

// 设置成就解锁回调
void achievement_set_callback(achievement_unlocked_cb_t cb);

// 获取新解锁的成就（用于UI轮询），返回-1表示没有新成就
int achievement_poll_new_unlock(struct achievement_state *state);

#ifdef __cplusplus
}
#endif
