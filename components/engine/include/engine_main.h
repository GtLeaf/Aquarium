#pragma once

#include "esp_err.h"
#include "species_data.h"

#ifdef __cplusplus
extern "C" {
#endif

// 游戏状态
enum game_state {
    STATE_BOOT = 0,
    STATE_TITLE,
    STATE_TANK_VIEW,
    STATE_EVENT_POPUP,
    STATE_AMBIENT_MODE,
};

// 全局游戏上下文
struct game_context {
    enum game_state state;
    struct game_save save;
    uint32_t frame_count;
    uint32_t state_timer_ms;
    bool dirty; // 数据变更标记，用于触发存档
};

// 获取全局游戏上下文
struct game_context* engine_get_context(void);

esp_err_t engine_init(void);
void engine_tick(void);
void engine_set_state(enum game_state new_state);

// 标记数据变更（触发自动存档）
void engine_mark_dirty(void);

// 离线产出计算
struct offline_reward {
    uint32_t offline_seconds;
    uint32_t coins_earned;
    uint8_t  creatures_alive;
    bool     capped; // 是否达到上限
};

struct offline_reward engine_calc_offline_reward(struct game_save *save, uint32_t now_timestamp);
void engine_apply_offline_reward(struct game_save *save, const struct offline_reward *reward);

// 获取事件触发器状态（供 UI 查询）
struct event_trigger_state* engine_get_event_state(void);

// 应用事件奖励并解锁图鉴
bool engine_apply_event_reward(struct game_save *save, uint8_t event_id);

// 解锁物种到图鉴
void engine_unlock_species(struct game_save *save, uint8_t species_id);

// 检查物种是否已解锁
bool engine_is_species_unlocked(const struct game_save *save, uint8_t species_id);

// 商店购买
bool engine_buy_species(struct game_save *save, uint8_t species_id);

// 缸等级升级
bool engine_upgrade_tank(struct game_save *save);
uint32_t engine_get_upgrade_cost(uint8_t current_level);

// 生物繁殖（成体且心情>80时概率繁殖）
void engine_try_breed(struct game_save *save);

// 重置游戏数据
void engine_reset_game(struct game_save *save);

#ifdef __cplusplus
}
#endif