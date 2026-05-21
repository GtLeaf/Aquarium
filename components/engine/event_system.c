#include "event_system.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <time.h>

static const char *TAG = "event_system";

// 事件冷却和每日上限检查
static bool can_trigger(const struct event_trigger_state *state, const struct event_def *ev, uint32_t now)
{
    if (!state || !ev) return false;

    uint8_t idx = ev->id - 1;
    if (idx >= 24) return false;

    // 每日上限检查
    if (state->daily_trigger_count[idx] >= ev->daily_cap) return false;

    // 冷却检查
    if (ev->cooldown_min > 0) {
        uint32_t cooldown_sec = ev->cooldown_min * 60;
        if (now - state->last_trigger_time[idx] < cooldown_sec) return false;
    }

    return true;
}

// 根据权重随机选择一个可触发的事件
static const struct event_def* pick_random_event(const struct event_trigger_state *state, uint32_t now)
{
    uint8_t count = event_get_count();
    if (count == 0) return NULL;

    // 收集可触发事件及其权重
    uint32_t weights[24] = {0};
    uint32_t total_weight = 0;
    uint8_t valid_count = 0;

    for (uint8_t i = 0; i < count; i++) {
        const struct event_def *ev = event_get_by_id(i + 1);
        if (ev && can_trigger(state, ev, now)) {
            weights[i] = ev->rarity;
            total_weight += ev->rarity;
            valid_count++;
        }
    }

    if (valid_count == 0 || total_weight == 0) return NULL;

    // 权重随机
    uint32_t pick = esp_random() % total_weight;
    uint32_t accum = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (weights[i] > 0) {
            accum += weights[i];
            if (pick < accum) {
                return event_get_by_id(i + 1);
            }
        }
    }
    return NULL;
}

void event_system_init(struct event_trigger_state *state)
{
    if (!state) return;
    memset(state, 0, sizeof(struct event_trigger_state));
    ESP_LOGI(TAG, "Event system initialized");
}

void event_system_daily_reset(struct event_trigger_state *state, uint16_t day_of_year)
{
    if (!state) return;
    memset(state->daily_trigger_count, 0, sizeof(state->daily_trigger_count));
    state->total_today = 0;
    state->day_of_year = day_of_year;
    ESP_LOGI(TAG, "Daily reset, day=%d", day_of_year);
}

void event_system_tick(struct event_trigger_state *state, struct game_save *save, uint32_t now)
{
    if (!state || !save) return;

    // 每日重置检查
    struct tm *tm_info = localtime((time_t *)&now);
    uint16_t current_day = tm_info ? tm_info->tm_yday : 0;
    if (current_day != state->day_of_year) {
        event_system_daily_reset(state, current_day);
        save->day_of_year = (uint8_t)current_day;
        save->today_events = 0;
    }

    // 全局每日上限：最多 5 个事件/天
    if (state->total_today >= 5) return;
    if (save->today_events >= 5) return;

    // 活跃事件槽位检查
    if (state->active_count >= MAX_ACTIVE_EVENTS) return;

    // 触发概率：每 10 分钟约 15% 概率检查一次（简化：每秒 0.025%）
    // 实际：每 600 秒检查一次，30% 概率触发
    static uint32_t check_timer = 0;
    check_timer++;
    if (check_timer < 600) return; // 每 10 分钟检查一次
    check_timer = 0;

    uint32_t roll = esp_random() % 100;
    if (roll >= 30) return; // 70% 不触发

    const struct event_def *ev = pick_random_event(state, now);
    if (!ev) return;

    // 添加到活跃事件
    for (int i = 0; i < MAX_ACTIVE_EVENTS; i++) {
        if (state->active[i].event_id == 0) {
            state->active[i].event_id = ev->id;
            state->active[i].trigger_time = now;
            state->active[i].expiry_time = now + 3600; // 1小时后过期
            state->active[i].viewed = false;
            state->active_count++;

            uint8_t idx = ev->id - 1;
            state->last_trigger_time[idx] = now;
            state->daily_trigger_count[idx]++;
            state->total_today++;
            save->today_events = state->total_today;

            ESP_LOGI(TAG, "Event triggered: [%d] %s, active=%d/%d",
                     ev->id, ev->name, state->active_count, MAX_ACTIVE_EVENTS);
            break;
        }
    }
}

bool event_system_has_pending(const struct event_trigger_state *state)
{
    if (!state) return false;
    for (int i = 0; i < MAX_ACTIVE_EVENTS; i++) {
        if (state->active[i].event_id != 0 && !state->active[i].viewed) {
            return true;
        }
    }
    return false;
}

const struct active_event* event_system_get_next_pending(struct event_trigger_state *state)
{
    if (!state) return NULL;
    for (int i = 0; i < MAX_ACTIVE_EVENTS; i++) {
        if (state->active[i].event_id != 0 && !state->active[i].viewed) {
            return &state->active[i];
        }
    }
    return NULL;
}

void event_system_mark_viewed(struct event_trigger_state *state, uint8_t event_id)
{
    if (!state) return;
    for (int i = 0; i < MAX_ACTIVE_EVENTS; i++) {
        if (state->active[i].event_id == event_id) {
            state->active[i].viewed = true;
            ESP_LOGI(TAG, "Event %d marked as viewed", event_id);
            break;
        }
    }
}

struct event_reward event_system_get_reward(uint8_t event_id)
{
    struct event_reward reward = {0};
    const struct event_def *ev = event_get_by_id(event_id);
    if (!ev) return reward;

    switch (event_id) {
        // 礼物事件 - 给光合币
        case 1:  reward.coins = 50; break;   // 海鸥来访
        case 2:  reward.coins = 30; break;   // 漂流瓶
        case 3:  reward.coins = 100; break;  // 新手周礼包
        case 4:  reward.coins = 80; break;   // 半月扶持
        case 5:  reward.coins = 25; break;   // 贝壳礼物
        case 6:  reward.coins = 20; break;   // 海星礼物
        case 7:  reward.coins = 20; break;   // 珊瑚碎片

        // 生物事件 - 给新物种
        case 8:  reward.species_id = 6; break;   // 黑壳虾
        case 9:  reward.species_id = 10; break;  // 灯科鱼
        case 10: reward.species_id = 13; break;  // 小丑鱼
        case 11: reward.species_id = 17; break;  // 红绿灯
        case 12: reward.species_id = 19; break;  // 神仙鱼

        // 环境事件 - 给营养或环境变化
        case 13: reward.nutrients = 20; break;  // 下雨日
        case 14: reward.nutrients = 10; break;  // 雷暴日
        case 15: reward.nutrients = 15; break;  // 雪天
        case 16: reward.nutrients = 30; break;  // 营养雨
        case 17: reward.coins = 15; break;      // 晨光
        case 18: reward.coins = 15; break;      // 日落

        // 氛围事件 - 少量币
        case 19: reward.coins = 10; break;  // 星夜
        case 20: reward.coins = 10; break;  // 微风
        case 21: reward.coins = 10; break;  // 月光
        case 22: reward.coins = 15; break;  // 彩虹

        // 保底事件 - 高概率新物种
        case 23: reward.species_id = 11; break; // 孔雀鱼 (新手周)
        case 24: reward.species_id = 12; break; // 斗鱼 (半月)
    }
    return reward;
}
