#include "achievement_system.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "achievement";

static achievement_unlocked_cb_t s_unlock_cb = NULL;
static uint64_t s_new_unlocks = 0; // 新解锁的成就位图（待UI处理）

static const char *achv_names[ACHV_COUNT] = {
    "First Breed",
    "Collector I",
    "Master Collector",
    "Max Tank",
    "Wealthy",
    "Survivor",
    "Giant Keeper",
    "Event Master",
};

static const char *achv_descs[ACHV_COUNT] = {
    "Breed your first creature",
    "Collect 10 species",
    "Collect all 30 species",
    "Upgrade tank to max level",
    "Hold 1000 photosynth coins",
    "Survive for 7 days",
    "Raise a creature to giant stage",
    "Trigger 50 events",
};

void achievement_init(struct achievement_state *state)
{
    if (!state) return;
    memset(state, 0, sizeof(struct achievement_state));
}

void achievement_check(struct achievement_state *state, const struct game_save *save)
{
    if (!state || !save) return;

    // 统计已解锁物种数
    int unlocked_count = 0;
    for (int i = 0; i < MAX_SPECIES; i++) {
        if (save->species_unlocked & (1ULL << i)) unlocked_count++;
    }

    // 检查是否有巨型生物
    bool has_giant = false;
    for (int i = 0; i < save->creature_count; i++) {
        if (save->creatures[i].stage == STAGE_GIANT) {
            has_giant = true;
            break;
        }
    }

    // 更新统计
    if (save->photosynth_coins > state->max_coins_held) {
        state->max_coins_held = save->photosynth_coins;
    }
    state->days_survived = save->play_days_total;

    // 检查成就
    struct {
        uint8_t id;
        bool unlocked;
    } checks[] = {
        {ACHV_FIRST_BREED,    state->breed_count > 0},
        {ACHV_COLLECT_10,     unlocked_count >= 10},
        {ACHV_COLLECT_ALL,    unlocked_count >= MAX_SPECIES},
        {ACHV_TANK_MAX,       save->tank_level >= 5},
        {ACHV_COINS_1000,     state->max_coins_held >= 1000},
        {ACHV_SURVIVE_7D,     state->days_survived >= 7},
        {ACHV_GIANT_CREATURE, has_giant},
        {ACHV_EVENT_50,       state->event_triggered >= 50},
    };

    for (int i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        if (checks[i].unlocked && !achievement_is_unlocked(state, checks[i].id)) {
            state->achievements_unlocked |= (1ULL << checks[i].id);
            s_new_unlocks |= (1ULL << checks[i].id);
            ESP_LOGI(TAG, "Achievement unlocked: %s - %s",
                     achv_names[checks[i].id], achv_descs[checks[i].id]);
            // 触发回调
            if (s_unlock_cb) {
                s_unlock_cb(checks[i].id, achv_names[checks[i].id], achv_descs[checks[i].id]);
            }
        }
    }
}

bool achievement_is_unlocked(const struct achievement_state *state, uint8_t achv_id)
{
    if (!state || achv_id >= ACHV_COUNT) return false;
    return (state->achievements_unlocked & (1ULL << achv_id)) != 0;
}

const char* achievement_get_name(uint8_t achv_id)
{
    if (achv_id >= ACHV_COUNT) return "???";
    return achv_names[achv_id];
}

const char* achievement_get_desc(uint8_t achv_id)
{
    if (achv_id >= ACHV_COUNT) return "???";
    return achv_descs[achv_id];
}

void achievement_set_callback(achievement_unlocked_cb_t cb)
{
    s_unlock_cb = cb;
}

int achievement_poll_new_unlock(struct achievement_state *state)
{
    if (!state || s_new_unlocks == 0) return -1;

    for (int i = 0; i < ACHV_COUNT; i++) {
        if (s_new_unlocks & (1ULL << i)) {
            s_new_unlocks &= ~(1ULL << i); // 清除该位
            return i;
        }
    }
    return -1;
}
