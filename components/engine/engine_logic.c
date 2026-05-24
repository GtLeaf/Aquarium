#include "engine_logic.h"
#include "species_data.h"
#include "hal_imu.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <math.h>

static const char *TAG = "engine_logic";

// 逻辑帧计数器：60帧(ENGINE_TICK_MS=16)才执行一次1秒逻辑
static uint32_t s_logic_tick = 0;
#define LOGIC_INTERVAL  (1000 / ENGINE_TICK_MS)  // ≈62帧 = 1秒

// ============================================================
// §4.3 环境层更新 (每秒调用一次)
// ============================================================
static void update_environment(struct game_context *ctx)
{
    struct environment *env = &ctx->save.env;

    // 总时间 +1 秒
    env->total_seconds++;

    // 日夜循环：每 5 分钟切换
    if (env->total_seconds % 300 == 0) {
        env->is_daytime = !env->is_daytime;
    }

    // §4.3 阳光：白天每分钟 +1，夜晚每2分钟 -1
    if (env->total_seconds % 60 == 0) {
        if (env->is_daytime) {
            if (env->sunlight < 100) env->sunlight++;
        }
    }
    if (!env->is_daytime && (env->total_seconds % 120 == 0)) {
        if (env->sunlight > 0) env->sunlight--;
    }

    // §4.4 algae_mass = 缸内所有 L1 植物 size 之和（只读快照，驱动产氧）
    if (env->total_seconds % 10 == 0) {
        uint16_t total_plant_size = 0;
        for (int i = 0; i < ctx->save.creature_count; i++) {
            const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
            if (sp && sp->trophic_level == TROPHIC_L1) {
                total_plant_size += ctx->save.creatures[i].size;
            }
        }
        // 映射：总 size 200 → algae_mass 100
        env->algae_mass = (uint8_t)(total_plant_size > 200 ? 100 : total_plant_size * 100 / 200);
    }

    // §4.4 L1 植物生长：每分钟，根据 light_pref + nutrient 增长
    if (env->total_seconds % 60 == 0) {
        for (int i = 0; i < ctx->save.creature_count; i++) {
            struct creature *plant = &ctx->save.creatures[i];
            const struct species_def *sp = species_get_by_id(plant->species_id);
            if (!sp || sp->trophic_level != TROPHIC_L1) continue;
            if (plant->size >= sp->size_cap) continue;

            // 光照需求匹配
            bool light_ok = false;
            switch (sp->light_pref) {
                case PREF_LOW:    light_ok = (env->sunlight > 10); break;
                case PREF_MEDIUM: light_ok = (env->sunlight > 30); break;
                case PREF_HIGH:   light_ok = (env->sunlight > 50); break;
                default:          light_ok = true; break;
            }

            // 营养需求匹配
            bool nut_ok = false;
            switch (sp->nutrient_need) {
                case PREF_LOW:    nut_ok = (env->nutrients > 5);  break;
                case PREF_MEDIUM: nut_ok = (env->nutrients > 15); break;
                case PREF_HIGH:   nut_ok = (env->nutrients > 30); break;
                default:          nut_ok = true; break;
            }

            if (light_ok && nut_ok) {
                // grow_factor 决定单次增长量
                uint8_t growth = sp->grow_factor > 0 ? sp->grow_factor : 1;
                if (plant->size + growth <= sp->size_cap) {
                    plant->size += growth;
                } else {
                    plant->size = sp->size_cap;
                }
                // 消耗营养
                uint8_t cost = (sp->nutrient_need == PREF_HIGH) ? 3 :
                               (sp->nutrient_need == PREF_MEDIUM) ? 2 : 1;
                if (env->nutrients >= cost) env->nutrients -= cost;
                else env->nutrients = 0;
            }
        }
    }

    // §4.3 氧气平衡：L1 光合产氧，动物按 oxygen_demand 耗氧
    if (env->total_seconds % 10 == 0) {
        int16_t o2 = env->oxygen;
        // 产氧：algae_mass / 10（每10秒）
        o2 += (env->algae_mass / 10);
        // 耗氧：非L1生物 oxygen_demand / 5（每10秒）
        for (int i = 0; i < ctx->save.creature_count; i++) {
            const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
            if (sp && sp->trophic_level != TROPHIC_L1) {
                o2 -= (sp->oxygen_demand / 5);
            }
        }
        if (o2 > 100) o2 = 100;
        if (o2 < 0) o2 = 0;
        env->oxygen = (uint8_t)o2;
    }

    ctx->dirty = true;
}

// ============================================================
// §4.5/4.6/4.7 觅食AI
// ============================================================

// 曼哈顿距离
static uint16_t creature_distance(const struct creature *a, const struct creature *b)
{
    int16_t dx = (int16_t)a->pos_x - (int16_t)b->pos_x;
    int16_t dy = (int16_t)a->pos_y - (int16_t)b->pos_y;
    return (uint16_t)(abs(dx) + abs(dy));
}

// 检查 species_id 是否在食谱数组中
static bool is_in_diet(const uint8_t diet[MAX_PREY_SLOTS], uint8_t species_id)
{
    for (int k = 0; k < MAX_PREY_SLOTS; k++) {
        if (diet[k] == 0) break;
        if (diet[k] == species_id) return true;
    }
    return false;
}

// ─── L2 觅食：啃食 L1 植物实例 ───
static void forage_l2(struct game_context *ctx, struct creature *c, const struct species_def *sp)
{
    if (c->hunger < 20) return; // 不太饿就不吃

    int best_idx = -1;
    uint16_t best_dist = 255;

    for (int j = 0; j < ctx->save.creature_count; j++) {
        struct creature *plant = &ctx->save.creatures[j];
        if (plant == c) continue;
        if (plant->state != 0) continue;

        // 必须是 L1 植物
        const struct species_def *plant_sp = species_get_by_id(plant->species_id);
        if (!plant_sp || plant_sp->trophic_level != TROPHIC_L1) continue;

        // 检查主食 eats[] 或备选 alt_eats[]
        bool can_eat = is_in_diet(sp->eats, plant->species_id) ||
                       is_in_diet(sp->alt_eats, plant->species_id);
        if (!can_eat) continue;

        uint16_t dist = creature_distance(c, plant);
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = j;
        }
    }

    if (best_idx >= 0) {
        struct creature *plant = &ctx->save.creatures[best_idx];

        // 朝植物移动（L2 速度较慢）
        if (plant->pos_x > c->pos_x) { c->vel_x = 1; c->facing_right = true; }
        else if (plant->pos_x < c->pos_x) { c->vel_x = -1; c->facing_right = false; }
        if (plant->pos_y > c->pos_y) c->vel_y = 1;
        else if (plant->pos_y < c->pos_y) c->vel_y = -1;

        // 接触判定：距离 < 10 才啃食
        if (best_dist < 10) {
            // 啃食效果
            uint8_t relief = 5; // 基础饥饿缓解
            if (c->hunger >= relief) c->hunger -= relief;
            else c->hunger = 0;
            c->growth_pts++;

            // 植物缩小
            if (plant->size > 1) {
                plant->size--;
            } else {
                // 植物被吃光 → 释放少量营养到环境，移除实例
                const struct species_def *plant_sp = species_get_by_id(plant->species_id);
                if (plant_sp) {
                    ctx->save.env.nutrients += plant_sp->size_base / 3;
                    if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
                }
                if (best_idx < ctx->save.creature_count - 1) {
                    ctx->save.creatures[best_idx] = ctx->save.creatures[ctx->save.creature_count - 1];
                }
                ctx->save.creature_count--;
                ESP_LOGI(TAG, "L2 %s ate a plant completely", sp->name);
            }
        }
    } else {
        // 无猎物 → 备选：环境 nutrient 缓解少量饥饿（效率低，每30秒-1）
        if (c->hunger > 40 && ctx->save.env.nutrients > 20) {
            if (ctx->save.env.total_seconds % 30 == 0) {
                if (c->hunger > 0) c->hunger--;
                ctx->save.env.nutrients--;
            }
        }
    }
}

// ─── 猎物逃跑AI：检测附近捕食者，反方向逃跑 ───
static void prey_flee(struct game_context *ctx, struct creature *prey, const struct species_def *prey_sp)
{
    // 只有正常态才逃跑
    if (prey->state != 0) return;
    // L1 植物不逃、L4 不需要逃
    if (prey_sp->trophic_level == TROPHIC_L1) return;
    if (prey_sp->trophic_level >= TROPHIC_L4A) return;

    // 扫描附近是否有捕食者正在靠近
    uint8_t flee_radius = 20; // 感知半径
    int threat_idx = -1;
    uint16_t closest_dist = 255;

    for (int j = 0; j < ctx->save.creature_count; j++) {
        struct creature *hunter = &ctx->save.creatures[j];
        if (hunter == prey) continue;
        if (hunter->state != 0) continue;

        const struct species_def *hunter_sp = species_get_by_id(hunter->species_id);
        if (!hunter_sp) continue;

        // 只怕比自己营养级高的，或同级但体型大很多的
        bool is_threat = false;
        if (hunter_sp->trophic_level > prey_sp->trophic_level) {
            is_threat = true;
        } else if (hunter_sp->trophic_level == prey_sp->trophic_level &&
                   hunter_sp->id != prey_sp->id &&
                   hunter->size > prey->size * 3 / 2) {
            is_threat = true;
        }
        if (!is_threat) continue;

        // 只有捕食者在饥饿状态才构成威胁
        if (hunter->hunger < 30) continue;

        uint16_t dist = creature_distance(prey, hunter);
        if (dist < flee_radius && dist < closest_dist) {
            closest_dist = dist;
            threat_idx = j;
        }
    }

    if (threat_idx < 0) return; // 没有威胁

    struct creature *hunter = &ctx->save.creatures[threat_idx];

    // 反方向逃跑（速度=2，比 L3 追猎慢但比普通游动快）
    int8_t flee_speed = 2;
    if (hunter->pos_x > prey->pos_x) { prey->vel_x = -flee_speed; prey->facing_right = false; }
    else if (hunter->pos_x < prey->pos_x) { prey->vel_x = flee_speed; prey->facing_right = true; }
    if (hunter->pos_y > prey->pos_y) prey->vel_y = -flee_speed;
    else if (hunter->pos_y < prey->pos_y) prey->vel_y = flee_speed;

    // 逃跑时随机偏移（避免直线逃跑被预判）
    if ((esp_random() % 100) < 30) {
        prey->vel_x += (int8_t)((esp_random() % 3) - 1);
        prey->vel_y += (int8_t)((esp_random() % 3) - 1);
    }
}

// ─── L3/L4 捕食AI：eats[] + alt_eats[] + 体型规则 + L4同级捕食 ───
static void forage_predator(struct game_context *ctx, struct creature *c, const struct species_def *sp)
{
    if (c->hunger < 30) return;

    uint8_t hunt_radius = (sp->trophic_level >= TROPHIC_L4A) ? 50 : 35;
    int best_idx = -1;
    uint16_t best_dist = 255;

    for (int j = 0; j < ctx->save.creature_count; j++) {
        struct creature *prey = &ctx->save.creatures[j];
        if (prey == c) continue;
        if (prey->state != 0) continue;

        const struct species_def *prey_sp = species_get_by_id(prey->species_id);
        if (!prey_sp) continue;

        // 不吃 L1 植物
        if (prey_sp->trophic_level == TROPHIC_L1) continue;

        bool can_eat = false;

        // 规则1: eats[] 主食列表
        if (is_in_diet(sp->eats, prey->species_id)) {
            can_eat = true;
        }

        // 规则2: alt_eats[] 备选列表
        if (!can_eat && is_in_diet(sp->alt_eats, prey->species_id)) {
            can_eat = true;
        }

        // 规则3: 低营养级 + 体型 ≤ 自身70%
        if (!can_eat && prey_sp->trophic_level < sp->trophic_level) {
            uint8_t size_threshold = (uint8_t)(c->size * 7 / 10);
            if (prey->size <= size_threshold) can_eat = true;
        }

        // 规则4: L4 可捕食非同种的较小 L4（体型 ≤ 自身60%）
        if (!can_eat &&
            (sp->trophic_level == TROPHIC_L4A || sp->trophic_level == TROPHIC_L4B) &&
            (prey_sp->trophic_level == TROPHIC_L4A || prey_sp->trophic_level == TROPHIC_L4B) &&
            prey_sp->id != sp->id)  // 非同种
        {
            uint8_t size_threshold = (uint8_t)(c->size * 6 / 10); // ≤ 60%
            if (prey->size <= size_threshold) can_eat = true;
        }

        if (!can_eat) continue;

        uint16_t dist = creature_distance(c, prey);
        if (dist < hunt_radius && dist < best_dist) {
            best_dist = dist;
            best_idx = j;
        }
    }

    if (best_idx >= 0) {
        struct creature *prey = &ctx->save.creatures[best_idx];

        // 朝猎物移动（L4 速度更快）
        int8_t speed = (sp->trophic_level >= TROPHIC_L4A) ? 3 : 2;
        if (prey->pos_x > c->pos_x) { c->vel_x = speed; c->facing_right = true; }
        else if (prey->pos_x < c->pos_x) { c->vel_x = -speed; c->facing_right = false; }
        if (prey->pos_y > c->pos_y) c->vel_y = (int8_t)(speed / 2 + 1);
        else if (prey->pos_y < c->pos_y) c->vel_y = (int8_t)(-(speed / 2 + 1));

        // 接触判定：距离 < 8 执行捕食
        if (best_dist < 8) {
            // 捕食收益：按猎物体型计算
            uint8_t relief = (prey->size / 3) + 5;
            if (relief > 30) relief = 30;
            if (c->hunger >= relief) c->hunger -= relief;
            else c->hunger = 0;
            c->growth_pts += 3;

            // 猎物被吃 → 释放营养到环境
            const struct species_def *prey_sp = species_get_by_id(prey->species_id);
            if (prey_sp) {
                uint8_t nut_release = prey_sp->size_base / 4;
                ctx->save.env.nutrients += nut_release;
                if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
            }

            // 移除猎物
            if (best_idx < ctx->save.creature_count - 1) {
                ctx->save.creatures[best_idx] = ctx->save.creatures[ctx->save.creature_count - 1];
            }
            ctx->save.creature_count--;
            ESP_LOGI(TAG, "Predation: %s caught prey, hunger=%d", sp->name, c->hunger);
        }
    } else {
        // 无猎物 → nutrient 备选（效率极低，每60秒-1饥饿 -2营养）
        if (c->hunger > 60 && ctx->save.env.nutrients > 15) {
            if (ctx->save.env.total_seconds % 60 == 0) {
                if (c->hunger > 0) c->hunger--;
                if (ctx->save.env.nutrients >= 2) ctx->save.env.nutrients -= 2;
                else ctx->save.env.nutrients = 0;
            }
        }
    }
}

// ============================================================
// §4.2 生物行为更新 (每秒调用一次)
// ============================================================
static void update_creatures(struct game_context *ctx)
{
    for (int i = 0; i < ctx->save.creature_count; i++) {
        struct creature *c = &ctx->save.creatures[i];
        const struct species_def *sp = species_get_by_id(c->species_id);
        if (!sp) continue;

        // 年龄 +1秒
        c->age_seconds++;

        // ─── 饥饿增长 ───
        // hunger_rate = 次/分钟 → 每 (60/hunger_rate) 秒 hunger+1
        // L1 植物不饥饿
        if (sp->trophic_level != TROPHIC_L1 && sp->hunger_rate > 0) {
            uint16_t interval = 60 / sp->hunger_rate;
            if (interval < 1) interval = 1;
            if ((c->age_seconds % interval) == 0) {
                if (c->hunger < 100) c->hunger++;
            }
        }

        // ─── 逃跑AI（每2秒检测一次） ───
        if (c->state == 0 && (c->age_seconds % 2 == 0)) {
            prey_flee(ctx, c, sp);
        }

        // ─── 觅食AI（每3秒执行一次，避免过于频繁） ───
        if (c->state == 0 && (c->age_seconds % 3 == 0)) {
            switch (sp->trophic_level) {
                case TROPHIC_L2:
                    forage_l2(ctx, c, sp);
                    break;
                case TROPHIC_L3:
                case TROPHIC_L4A:
                case TROPHIC_L4B:
                    forage_predator(ctx, c, sp);
                    break;
                default:
                    break; // L1 不觅食
            }
        }

        // ─── 随机游动（非追猎状态且静止时） ───
        if (c->state == 0 && c->vel_x == 0 && c->vel_y == 0) {
            if ((c->age_seconds + i * 7) % 5 == 0) {
                c->vel_x = (int8_t)((esp_random() % 5) - 2);
                c->vel_y = (int8_t)((esp_random() % 3) - 1);
                c->facing_right = (c->vel_x >= 0);
            }
        }

        // ─── 应用速度 (L1 植物不移动) ───
        if (sp->trophic_level != TROPHIC_L1) {
            c->pos_x += c->vel_x;
            c->pos_y += c->vel_y;

            // 水阻力衰减（每2秒）
            if (c->age_seconds % 2 == 0) {
                if (c->vel_x > 0) c->vel_x--;
                else if (c->vel_x < 0) c->vel_x++;
                if (c->vel_y > 0) c->vel_y--;
                else if (c->vel_y < 0) c->vel_y++;
            }

            // 边界限制
            if (c->pos_x < 0) { c->pos_x = 0; c->vel_x = 1; }
            if (c->pos_x > 120) { c->pos_x = 120; c->vel_x = -1; }
            if (c->pos_y < 0) { c->pos_y = 0; c->vel_y = 1; }
            if (c->pos_y > 120) { c->pos_y = 120; c->vel_y = -1; }
        }

        // ─── §4.8 成长：饱食(hunger<30)时累积 growth_pts ───
        if (sp->trophic_level != TROPHIC_L1 && c->hunger < 30 && c->stage < STAGE_GIANT) {
            // 每30秒自然增长1点
            if (c->age_seconds % 30 == 0) {
                c->growth_pts++;
            }
            // 达到50点 → 体型+5，检查阶段升级
            if (c->growth_pts >= 50) {
                c->growth_pts = 0;
                if (c->size + 5 <= sp->size_cap) {
                    c->size += 5;
                } else {
                    c->size = sp->size_cap;
                }
                // 阶段升级判定
                if (c->stage == STAGE_JUVENILE && c->size >= sp->size_base * 2) {
                    c->stage = STAGE_SUBADULT;
                } else if (c->stage == STAGE_SUBADULT && c->size >= sp->size_base * 3) {
                    c->stage = STAGE_ADULT;
                } else if (c->stage == STAGE_ADULT && c->size >= sp->size_cap * 9 / 10) {
                    c->stage = STAGE_GIANT;
                }
                ESP_LOGI(TAG, "Growth: creature %d size=%d stage=%d", i, c->size, c->stage);
            }
        }

        // ─── §4.10 柔性死亡 ───
        // L1 通过被啃光 size 来移除，不走饥饿死亡
        if (sp->trophic_level != TROPHIC_L1 && c->hunger > 95) {
            if (c->state == 0) {
                c->state = 1; // 进入睡眠态
                c->sleep_timer = 30 * 60; // 30分钟观察期
                ESP_LOGW(TAG, "Creature %d entering sleep (starvation)", i);
            } else if (c->state == 1) {
                if (c->sleep_timer > 0) {
                    c->sleep_timer--;
                } else {
                    c->state = 2;
                    c->sleep_timer = 3600; // 1h 死亡倒计时
                }
            } else if (c->state == 2) {
                if (c->sleep_timer > 0) {
                    c->sleep_timer--;
                } else {
                    // 温柔消失 → 营养回馈环境
                    ctx->save.env.nutrients += 20;
                    if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
                    if (i < ctx->save.creature_count - 1) {
                        ctx->save.creatures[i] = ctx->save.creatures[ctx->save.creature_count - 1];
                    }
                    ctx->save.creature_count--;
                    i--; // 当前槽被替换，重新处理
                    ESP_LOGW(TAG, "Creature died peacefully, nutrients +20");
                }
            }
        } else if ((c->state == 1 || c->state == 2) && c->hunger < 80) {
            // 喂饱后苏醒
            c->state = 0;
            c->sleep_timer = 0;
        }

        // ─── 低氧应激 ───
        if (sp->trophic_level != TROPHIC_L1 && ctx->save.env.oxygen < 20) {
            // 低氧时心情下降，加速饥饿
            if (c->age_seconds % 10 == 0) {
                if (c->mood > 0) c->mood--;
                if (c->hunger < 100) c->hunger++;
            }
        }
    }

    ctx->dirty = true;
}

// ============================================================
// §4.11 稳态修复 (每30秒)
// ============================================================
static void update_homeostasis(struct game_context *ctx)
{
    if (ctx->save.env.total_seconds % 30 != 0) return;

    struct environment *env = &ctx->save.env;

    // ─── 藻类爆发控制 ───
    // algae_mass > 80 且无 L2 时，最大 L1 自然老化
    if (env->algae_mass > 80) {
        uint8_t l2_count = 0;
        for (int i = 0; i < ctx->save.creature_count; i++) {
            const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
            if (sp && sp->trophic_level == TROPHIC_L2) l2_count++;
        }
        if (l2_count == 0) {
            int biggest = -1;
            uint8_t max_sz = 0;
            for (int i = 0; i < ctx->save.creature_count; i++) {
                const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
                if (sp && sp->trophic_level == TROPHIC_L1 && ctx->save.creatures[i].size > max_sz) {
                    max_sz = ctx->save.creatures[i].size;
                    biggest = i;
                }
            }
            if (biggest >= 0 && ctx->save.creatures[biggest].size > 1) {
                ctx->save.creatures[biggest].size -= 2;
                if (ctx->save.creatures[biggest].size < 1) ctx->save.creatures[biggest].size = 1;
            }
        }
    }

    // ─── 缸内生物过少 → 野生访客 ───
    if (ctx->save.creature_count < 3 && ctx->save.creature_count < MAX_CREATURES) {
        if (ctx->save.env.total_seconds % 300 == 0) {
            if ((esp_random() % 100) < 20) {
                uint8_t target_level = ((esp_random() % 2) == 0) ? TROPHIC_L2 : TROPHIC_L3;
                const struct species_def *sp = species_get_random(target_level, RARITY_COMMON);
                if (sp) {
                    struct creature *nc = &ctx->save.creatures[ctx->save.creature_count];
                    memset(nc, 0, sizeof(*nc));
                    nc->species_id = sp->id;
                    nc->stage = STAGE_JUVENILE;
                    nc->size = sp->size_base;
                    nc->pos_x = (int8_t)(esp_random() % 120);
                    nc->pos_y = (int8_t)(esp_random() % 120);
                    nc->hunger = 30;
                    nc->mood = sp->mood_base;
                    nc->state = 0;
                    ctx->save.creature_count++;
                    ESP_LOGI(TAG, "Wild visitor: %s drifted in!", sp->name);
                    ctx->dirty = true;
                }
            }
        }
    }

    // ─── 营养自然补充 ───
    // 有死亡有机物(隐含)：每5分钟 nutrients +2
    if (ctx->save.env.total_seconds % 300 == 0) {
        if (env->nutrients < 100) {
            env->nutrients += 2;
            if (env->nutrients > 100) env->nutrients = 100;
        }
    }

    // 夜间阳光自然降低是正常生态节奏，低光植物(PREF_LOW)适应夜间环境
    // 不做自动补光干预
}

// ============================================================
// 摇晃效果处理
// ============================================================
static void apply_shake_effect(struct game_context *ctx)
{
    static uint32_t s_imu_check_timer = 0;
    s_imu_check_timer += ENGINE_TICK_MS;
    if (s_imu_check_timer < 100) return; // 每100ms检测一次
    s_imu_check_timer = 0;

    shake_level_t shake = hal_imu_detect_shake();
    if (shake == SHAKE_NONE) return;

    enum shake_effect effect = hal_imu_get_shake_effect(shake);
    switch (effect) {
        case SHAKE_EFFECT_FEED:
            // 全体喂食：hunger -10，唤醒睡眠态
            for (int i = 0; i < ctx->save.creature_count; i++) {
                const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
                if (sp && sp->trophic_level == TROPHIC_L1) continue; // 植物跳过
                if (ctx->save.creatures[i].hunger >= 10) {
                    ctx->save.creatures[i].hunger -= 10;
                } else {
                    ctx->save.creatures[i].hunger = 0;
                }
                if (ctx->save.creatures[i].state == 1 || ctx->save.creatures[i].state == 2) {
                    ctx->save.creatures[i].state = 0;
                    ctx->save.creatures[i].sleep_timer = 0;
                }
            }
            ESP_LOGI(TAG, "Shake FEED: all creatures -10 hunger, wake sleepers");
            break;

        case SHAKE_EFFECT_OXYGEN:
            {
                uint16_t o2 = ctx->save.env.oxygen + 20;
                if (o2 > 100) o2 = 100;
                ctx->save.env.oxygen = (uint8_t)o2;
                ESP_LOGI(TAG, "Shake OXYGEN: +20");
            }
            break;

        case SHAKE_EFFECT_SCATTER:
            // L1 植物受冲击：20%概率缩小或移除
            for (int i = ctx->save.creature_count - 1; i >= 0; i--) {
                const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
                if (!sp || sp->trophic_level != TROPHIC_L1) continue;
                if ((esp_random() % 100) < 20) {
                    if (ctx->save.creatures[i].size > 1) {
                        ctx->save.creatures[i].size--;
                        ctx->save.env.nutrients += sp->size_base / 3;
                        if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
                    } else {
                        // size=1 被冲走
                        if (i < ctx->save.creature_count - 1) {
                            ctx->save.creatures[i] = ctx->save.creatures[ctx->save.creature_count - 1];
                        }
                        ctx->save.creature_count--;
                    }
                    ESP_LOGW(TAG, "Shake SCATTER: L1 plant damaged");
                }
            }
            break;

        default:
            break;
    }
    ctx->dirty = true;
}

// 倾斜效果：90度翻转触发水循环
static void apply_tilt_effect(struct game_context *ctx)
{
    float pitch, roll;
    hal_imu_get_tilt(&pitch, &roll);

    if (hal_imu_detect_water_cycle(pitch, roll)) {
        ctx->save.env.nutrients += 10;
        if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
        ESP_LOGI(TAG, "Water cycle: nutrients +10");
        ctx->dirty = true;
    }
}

// ============================================================
// 主入口：每帧调用（16ms），内部节流到每秒执行逻辑
// ============================================================
void engine_logic_update(struct game_context *ctx)
{
    if (!ctx) return;

    // IMU 效果每帧都检测（100ms 内部节流）
    apply_shake_effect(ctx);
    apply_tilt_effect(ctx);

    // 生态逻辑每秒执行一次
    s_logic_tick++;
    if (s_logic_tick < LOGIC_INTERVAL) return;
    s_logic_tick = 0;

    update_environment(ctx);
    update_creatures(ctx);
    update_homeostasis(ctx);
}
