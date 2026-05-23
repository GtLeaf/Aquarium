#include "engine_main.h"
#include "engine_logic.h"
#include "event_system.h"
#include "achievement_system.h"
#include "save_manager.h"
#include "hal_rtc.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <time.h>

static const char *TAG = "engine";
static struct game_context s_ctx = {0};
static struct event_trigger_state s_event_state = {0};
static struct achievement_state s_achv_state = {0};
static uint32_t s_tick_ms = 0;

struct game_context* engine_get_context(void)
{
    return &s_ctx;
}

// 缸等级对应离线收益上限（小时）和系数
static uint32_t tank_offline_cap_hours(uint8_t tank_level)
{
    switch (tank_level) {
        case 1: return 4;
        case 2: return 8;
        case 3: return 12;
        case 4: return 18;
        case 5: return 24;
        default: return 4;
    }
}

static uint32_t tank_offline_coefficient(uint8_t tank_level)
{
    // 基础系数：每只生物每小时 5 币
    return 5 + (tank_level - 1) * 2;
}

struct offline_reward engine_calc_offline_reward(struct game_save *save, uint32_t now_timestamp)
{
    struct offline_reward reward = {0};
    if (!save || save->offline_start == 0 || now_timestamp <= save->offline_start) {
        return reward;
    }

    reward.offline_seconds = now_timestamp - save->offline_start;
    uint32_t offline_hours = reward.offline_seconds / 3600;
    uint32_t cap_hours = tank_offline_cap_hours(save->tank_level);

    if (offline_hours > cap_hours) {
        offline_hours = cap_hours;
        reward.capped = true;
    }

    // 只计算存活生物（非睡眠态）
    uint8_t alive = 0;
    for (int i = 0; i < save->creature_count; i++) {
        if (save->creatures[i].state == 0) {
            alive++;
        }
    }
    reward.creatures_alive = alive;

    uint32_t coeff = tank_offline_coefficient(save->tank_level);
    reward.coins_earned = alive * coeff * offline_hours;

    ESP_LOGI(TAG, "Offline reward: %lu sec (%lu h), creatures=%d, coins=%lu, capped=%d",
             (unsigned long)reward.offline_seconds, (unsigned long)offline_hours,
             alive, (unsigned long)reward.coins_earned, reward.capped);
    return reward;
}

void engine_apply_offline_reward(struct game_save *save, const struct offline_reward *reward)
{
    if (!save || !reward) return;
    save->photosynth_coins += reward->coins_earned;
    save->offline_start = 0; // 清空离线计时
    ESP_LOGI(TAG, "Applied offline reward: +%lu coins, total=%lu",
             (unsigned long)reward->coins_earned,
             (unsigned long)save->photosynth_coins);
}

esp_err_t engine_init(void)
{
    ESP_LOGI(TAG, "Engine initializing...");

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = STATE_BOOT;

    // 尝试读取存档
    esp_err_t ret = save_gamesave_read(&s_ctx.save);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load save, using default");
        save_gamesave_init_default(&s_ctx.save);
    }

    // 计算离线收益
    time_t now = time(NULL);
    struct offline_reward reward = engine_calc_offline_reward(&s_ctx.save, (uint32_t)now);
    if (reward.coins_earned > 0) {
        engine_apply_offline_reward(&s_ctx.save, &reward);
        s_ctx.dirty = true;
    }

    // 记录新的离线开始时间
    s_ctx.save.offline_start = (uint32_t)now;

    s_ctx.state = STATE_TANK_VIEW;
    s_ctx.frame_count = 0;
    s_ctx.state_timer_ms = 0;

    // 初始化事件系统
    event_system_init(&s_event_state);

    // 初始化成就系统
    achievement_init(&s_achv_state);

    ESP_LOGI(TAG, "Engine ready, state=%d, creatures=%d, coins=%lu",
             s_ctx.state, s_ctx.save.creature_count,
             (unsigned long)s_ctx.save.photosynth_coins);
    return ESP_OK;
}

void engine_set_state(enum game_state new_state)
{
    if (s_ctx.state == new_state) return;
    ESP_LOGI(TAG, "State: %d -> %d", s_ctx.state, new_state);
    s_ctx.state = new_state;
    s_ctx.state_timer_ms = 0;
    engine_mark_dirty();
}

void engine_mark_dirty(void)
{
    s_ctx.dirty = true;
}

struct event_trigger_state* engine_get_event_state(void)
{
    return &s_event_state;
}

void engine_unlock_species(struct game_save *save, uint8_t species_id)
{
    if (!save || species_id == 0 || species_id > 64) return;
    uint64_t mask = 1ULL << (species_id - 1);
    if ((save->species_unlocked & mask) == 0) {
        save->species_unlocked |= mask;
        ESP_LOGI(TAG, "Species %d unlocked!", species_id);
        engine_mark_dirty();
    }
}

bool engine_is_species_unlocked(const struct game_save *save, uint8_t species_id)
{
    if (!save || species_id == 0 || species_id > 64) return false;
    return (save->species_unlocked & (1ULL << (species_id - 1))) != 0;
}

bool engine_apply_event_reward(struct game_save *save, uint8_t event_id)
{
    if (!save) return false;
    struct event_reward reward = event_system_get_reward(event_id);
    if (reward.coins > 0) {
        save->photosynth_coins += reward.coins;
        ESP_LOGI(TAG, "Event reward: +%lu coins", (unsigned long)reward.coins);
    }
    if (reward.nutrients > 0) {
        uint16_t nutrients = save->env.nutrients + reward.nutrients;
        if (nutrients > 100) nutrients = 100;
        save->env.nutrients = (uint8_t)nutrients;
        ESP_LOGI(TAG, "Event reward: +%d nutrients", reward.nutrients);
    }
    if (reward.species_id > 0) {
        engine_unlock_species(save, reward.species_id);
        // 自动添加到缸中（如果还有空位）
        if (save->creature_count < MAX_CREATURES) {
            const struct species_def *sp = species_get_by_id(reward.species_id);
            if (sp) {
                struct creature *c = &save->creatures[save->creature_count];
                c->species_id = reward.species_id;
                c->stage = STAGE_JUVENILE;
                c->size = sp->size_base;
                c->pos_x = (int8_t)(50 + (esp_random() % 50));
                c->pos_y = (int8_t)(50 + (esp_random() % 50));
                c->hunger = 30;
                c->mood = 80;
                c->state = 0;
                save->creature_count++;
                ESP_LOGI(TAG, "Event reward: new creature %s added", sp->name);
            }
        }
    }
    engine_mark_dirty();
    return true;
}

uint32_t engine_get_upgrade_cost(uint8_t current_level)
{
    switch (current_level) {
        case 1: return 500;
        case 2: return 1500;
        case 3: return 3000;
        case 4: return 5000;
        default: return 99999;
    }
}

bool engine_upgrade_tank(struct game_save *save)
{
    if (!save || save->tank_level >= 5) return false;
    uint32_t cost = engine_get_upgrade_cost(save->tank_level);
    if (save->photosynth_coins < cost) return false;

    save->photosynth_coins -= cost;
    save->tank_level++;
    ESP_LOGI(TAG, "Tank upgraded to L%d, cost=%lu", save->tank_level, (unsigned long)cost);
    engine_mark_dirty();
    return true;
}

uint32_t engine_get_species_price(uint8_t species_id)
{
    if (species_id == 0 || species_id > MAX_SPECIES) return 0;
    const struct species_def *sp = species_get_by_id(species_id);
    if (!sp) return 0;

    switch (sp->trophic_level) {
        case 1: return 50;
        case 2: return 100;
        case 3: return 150;
        case 4:
            switch (sp->rarity) {
                case RARITY_COMMON:   return 300;
                case RARITY_RARE:     return 500;
                case RARITY_LIMITED:  return 800;
                case RARITY_LONGTAIL: return 1000;
                default:              return 300;
            }
        default: return 100;
    }
}

bool engine_buy_species(struct game_save *save, uint8_t species_id)
{
    if (!save || species_id == 0 || species_id > MAX_SPECIES) return false;
    if (save->creature_count >= MAX_CREATURES) return false;

    const struct species_def *sp = species_get_by_id(species_id);
    if (!sp) return false;

    // 检查是否已解锁图鉴
    if (!engine_is_species_unlocked(save, species_id)) return false;

    // 检查同种数量上限
    uint8_t same_count = 0;
    for (int i = 0; i < save->creature_count; i++) {
        if (save->creatures[i].species_id == species_id) same_count++;
    }
    if (same_count >= sp->max_per_tank) return false;

    uint32_t price = engine_get_species_price(species_id);
    if (price == 0 || save->photosynth_coins < price) return false;

    save->photosynth_coins -= price;
    struct creature *c = &save->creatures[save->creature_count];
    c->species_id = species_id;
    c->stage = STAGE_JUVENILE;
    c->size = sp->size_base;
    c->pos_x = (int8_t)(30 + (esp_random() % 60));
    c->pos_y = (int8_t)(30 + (esp_random() % 60));
    c->hunger = 20;
    c->mood = 90;
    c->state = 0;
    save->creature_count++;

    ESP_LOGI(TAG, "Bought %s for %lu coins", sp->name, (unsigned long)price);
    engine_mark_dirty();
    return true;
}

void engine_try_breed(struct game_save *save)
{
    if (!save || save->creature_count >= MAX_CREATURES) return;

    // 每 5 分钟尝试一次繁殖
    static uint32_t breed_timer = 0;
    breed_timer++;
    if (breed_timer < 300) return; // 300 秒 = 5 分钟
    breed_timer = 0;

    // 寻找可繁殖的成对生物
    for (int i = 0; i < save->creature_count; i++) {
        struct creature *parent = &save->creatures[i];
        if (parent->stage != STAGE_ADULT && parent->stage != STAGE_GIANT) continue;
        if (parent->mood < 80) continue;
        if (parent->state != 0) continue;

        const struct species_def *sp = species_get_by_id(parent->species_id);
        if (!sp) continue;
        if (sp->trophic_level == TROPHIC_L4A || sp->trophic_level == TROPHIC_L4B) continue; // L4 不繁殖

        // 检查同种数量
        uint8_t same_count = 0;
        for (int j = 0; j < save->creature_count; j++) {
            if (save->creatures[j].species_id == parent->species_id) same_count++;
        }
        if (same_count >= sp->max_per_tank) continue;

        // 30% 概率繁殖
        if ((esp_random() % 100) < 30) {
            struct creature *baby = &save->creatures[save->creature_count];
            baby->species_id = parent->species_id;
            baby->stage = STAGE_JUVENILE;
            baby->size = sp->size_base / 2;
            if (baby->size < 3) baby->size = 3;
            baby->pos_x = parent->pos_x + (int8_t)((esp_random() % 10) - 5);
            baby->pos_y = parent->pos_y + (int8_t)((esp_random() % 10) - 5);
            baby->hunger = 10;
            baby->mood = 100;
            baby->state = 0;
            save->creature_count++;

            parent->mood -= 20; // 繁殖后心情下降
            ESP_LOGI(TAG, "Breed success: %s baby born!", sp->name);
            engine_mark_dirty();
            break; // 每次只繁殖一只
        }
    }
}

void engine_reset_game(struct game_save *save)
{
    if (!save) return;
    save_gamesave_delete();
    save_gamesave_init_default(save);
    ESP_LOGI(TAG, "Game reset to default");
}

void engine_tick(void)
{
    const uint32_t dt_ms = 16; // ~60 FPS
    s_tick_ms += dt_ms;
    s_ctx.frame_count++;
    s_ctx.state_timer_ms += dt_ms;

    // 生态逻辑每 1 秒执行一次（1000ms / 16ms ≈ 62 帧）
    static uint32_t ecology_timer = 0;
    ecology_timer += dt_ms;
    if (ecology_timer >= 1000) {
        ecology_timer = 0;
        engine_logic_update(&s_ctx);

        // 事件系统每秒 tick
        time_t now = time(NULL);
        event_system_tick(&s_event_state, &s_ctx.save, (uint32_t)now);

        // 繁殖尝试
        engine_try_breed(&s_ctx.save);

        // 成就检查（每 10 秒一次）
        static uint32_t achv_timer = 0;
        achv_timer++;
        if (achv_timer >= 10) {
            achv_timer = 0;
            achievement_check(&s_achv_state, &s_ctx.save);
        }
    }

    // 自动存档（每 1 分钟）
    save_auto_save_tick(dt_ms, &s_ctx.save);
}
