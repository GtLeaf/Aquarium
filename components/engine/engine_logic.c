#include "engine_logic.h"
#include "species_data.h"
#include "hal_imu.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <math.h>

static const char *TAG = "engine_logic";

// ============================================================
// §4.3 环境层更新
// ============================================================
static void update_environment(struct game_context *ctx)
{
    struct environment *env = &ctx->save.env;

    // 日夜循环（简化：每 5 分钟切换）
    env->total_seconds++;
    if (env->total_seconds % 300 == 0) {
        env->is_daytime = !env->is_daytime;
    }

    // §4.3 阳光：白天 +1/min，夜晚 -0.5/min
    // 心跳每秒一次，所以 +1/min = 每60s +1
    if (env->total_seconds % 60 == 0) {
        if (env->is_daytime) {
            if (env->sunlight < 100) env->sunlight++;
        } else {
            // -0.5/min => 每 120s -1
        }
    }
    if (!env->is_daytime && (env->total_seconds % 120 == 0)) {
        if (env->sunlight > 0) env->sunlight--;
    }

    // §4.3 营养：被 L1 消耗 -0.2/min·棵
    // 统计 L1 数量（包括藻类 algae_mass 作为虚拟棵数）
    uint8_t l1_count = 0;
    for (int i = 0; i < ctx->save.creature_count; i++) {
        const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
        if (sp && sp->trophic_level == TROPHIC_L1) l1_count++;
    }
    // algae_mass/20 作为等效 L1 棵数 (0-5棵)
    uint8_t total_l1 = l1_count + (env->algae_mass / 20);
    // -0.2/min·棵 => 每 5s 每棵 -0.0167 => 每 300s(5min) 每棵 -1
    if (total_l1 > 0 && (env->total_seconds % (300 / total_l1 + 1) == 0)) {
        if (env->nutrients > 0) env->nutrients--;
    }

    // §4.4 藻类 Logistic 生长: ΔBiomass = k1 × 阳光 × 营养 × (1 - Biomass/Capacity)
    // 每分钟计算一次
    if (env->total_seconds % 60 == 0) {
        uint16_t growth = (uint16_t)env->sunlight * env->nutrients * (100 - env->algae_mass) / 100000;
        if (growth < 1 && env->algae_mass < 50 && env->sunlight > 30 && env->nutrients > 30) {
            growth = 1;
        }
        if (env->algae_mass + growth > 100) growth = 100 - env->algae_mass;
        env->algae_mass += (uint8_t)growth;
    }

    // §4.3 氧气：L1 光合产氧，L2/L3/L4 按 oxygen_demand 消耗
    if (env->total_seconds % 10 == 0) {
        int16_t o2 = env->oxygen;
        // 产氧：藻类生物量 / 10
        o2 += (env->algae_mass / 10);
        // 耗氧：每个生物按 oxygen_demand
        for (int i = 0; i < ctx->save.creature_count; i++) {
            const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
            if (sp && sp->trophic_level != TROPHIC_L1) {
                o2 -= (sp->oxygen_demand / 5); // 每10s消耗 demand/5
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

// 计算两个生物之间的距离
static uint16_t creature_distance(const struct creature *a, const struct creature *b)
{
    int16_t dx = (int16_t)a->pos_x - (int16_t)b->pos_x;
    int16_t dy = (int16_t)a->pos_y - (int16_t)b->pos_y;
    return (uint16_t)(abs(dx) + abs(dy)); // 曼哈顿距离
}

// L2 啃食藻类 (L1 algae_mass)
static void forage_l2(struct game_context *ctx, struct creature *c)
{
    // §4.5: hunger每分钟+0.5 => 每120s +1
    // 觅食优先 L1 藻类
    if (c->hunger > 20 && ctx->save.env.algae_mass > 5) {
        // 进食：每秒 hunger -2，藻类 -0.1
        if (c->hunger >= 2) c->hunger -= 2;
        else c->hunger = 0;
        // 藻类减少（每10次减1防止过快）
        static uint8_t eat_counter = 0;
        eat_counter++;
        if (eat_counter >= 10) {
            eat_counter = 0;
            if (ctx->save.env.algae_mass > 0) ctx->save.env.algae_mass--;
        }
        // 成长点
        c->growth_pts++;
    }
}

// L3/L4 捕食AI：寻找半径R内体型 ≤ 自己 0.7 倍的可食目标
static void forage_predator(struct game_context *ctx, struct creature *c, const struct species_def *sp)
{
    if (c->hunger < 40) return; // 不饿就不捕食

    // §4.6: 寻找半径 R=40 内体型 ≤ 自己 0.7 倍的猎物
    uint8_t hunt_radius = 40;
    uint8_t size_threshold = (uint8_t)(c->size * 7 / 10);
    int best_idx = -1;
    uint16_t best_dist = 255;

    for (int j = 0; j < ctx->save.creature_count; j++) {
        struct creature *prey = &ctx->save.creatures[j];
        if (prey == c) continue;
        if (prey->state != 0) continue; // 跳过睡眠/死亡

        const struct species_def *prey_sp = species_get_by_id(prey->species_id);
        if (!prey_sp) continue;

        // 检查食物链关系
        bool can_eat = false;
        if (sp->trophic_level == TROPHIC_L3) {
            // L3 吃 L2，同级别只有体型差>=0.5倍时才吃
            if (prey_sp->trophic_level == TROPHIC_L2) {
                can_eat = true;
            } else if (prey_sp->trophic_level == TROPHIC_L3 && prey->size <= size_threshold) {
                can_eat = true; // 大鱼吃小鱼
            }
        } else if (sp->trophic_level == TROPHIC_L4A || sp->trophic_level == TROPHIC_L4B) {
            // L4 吃 L3，偶尔吃 L2
            if (prey_sp->trophic_level == TROPHIC_L3) {
                can_eat = true;
            } else if (prey_sp->trophic_level == TROPHIC_L2 && c->hunger > 80) {
                can_eat = true; // 很饿时才吃 L2
            }
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

        // 朝猎物移动
        if (prey->pos_x > c->pos_x) { c->vel_x = 2; c->facing_right = true; }
        else if (prey->pos_x < c->pos_x) { c->vel_x = -2; c->facing_right = false; }
        if (prey->pos_y > c->pos_y) c->vel_y = 1;
        else if (prey->pos_y < c->pos_y) c->vel_y = -1;

        // 接触判定：距离 < 8
        if (best_dist < 8) {
            // 进食成功
            if (c->hunger >= 15) c->hunger -= 15;
            else c->hunger = 0;
            c->growth_pts += 3;

            // 猎物被吃掉：释放营养
            const struct species_def *prey_sp = species_get_by_id(prey->species_id);
            if (prey_sp) {
                ctx->save.env.nutrients += prey_sp->food_value / 5;
                if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
            }

            // 移除猎物
            if (best_idx < ctx->save.creature_count - 1) {
                ctx->save.creatures[best_idx] = ctx->save.creatures[ctx->save.creature_count - 1];
            }
            ctx->save.creature_count--;
            ESP_LOGI(TAG, "Predation: %s ate prey, hunger=%d", sp->name, c->hunger);

            // §4.6: 吃饱后 30s 内不再捕食（通过 hunger < 40 的检查自然实现）
        }
    }
}

// ============================================================
// §4.2 生物行为更新（心跳每秒）
// ============================================================
static void update_creatures(struct game_context *ctx)
{
    for (int i = 0; i < ctx->save.creature_count; i++) {
        struct creature *c = &ctx->save.creatures[i];
        const struct species_def *sp = species_get_by_id(c->species_id);
        if (!sp) continue;

        // 年龄增长
        c->age_seconds++;

        // §4.5 饥饿增长：每分钟 +0.5 => 每 120s +1
        // L1 不饥饿
        if (sp->trophic_level != TROPHIC_L1) {
            if ((ctx->frame_count % 120) == (uint32_t)(i * 7 % 120)) {
                if (c->hunger < 100) c->hunger++;
            }
        }

        // 觅食AI
        if (c->state == 0) {
            switch (sp->trophic_level) {
                case TROPHIC_L2:
                    forage_l2(ctx, c);
                    break;
                case TROPHIC_L3:
                case TROPHIC_L4A:
                case TROPHIC_L4B:
                    forage_predator(ctx, c, sp);
                    break;
                default:
                    break;
            }
        }

        // 随机游动（没有在追猎时）
        if (c->state == 0 && c->vel_x == 0 && c->vel_y == 0) {
            // §4.6: 若无猎物 → 在缸内悠游（贝塞尔曲线随机路径简化为随机方向）
            if ((ctx->frame_count + i * 7) % 30 == 0) {
                c->vel_x = (int8_t)((esp_random() % 5) - 2);
                c->vel_y = (int8_t)((esp_random() % 5) - 2);
                c->facing_right = (c->vel_x >= 0);
            }
        }

        // 应用速度
        c->pos_x += c->vel_x;
        c->pos_y += c->vel_y;

        // 速度衰减（模拟水阻力）
        if ((ctx->frame_count + i) % 10 == 0) {
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

        // §4.8 成长：连续30分钟饱食(hunger<30) → growth_pts +1
        if (c->hunger < 30 && c->stage < STAGE_GIANT) {
            // 每 30s 检查一次（简化连续30min为累积growth_pts）
            if (c->age_seconds % 30 == 0) {
                c->growth_pts++;
            }
            if (c->growth_pts >= 50) {
                c->growth_pts = 0;
                if (c->size + 5 <= sp->size_max) {
                    c->size += 5;
                }
                // 阶段升级
                if (c->stage == STAGE_JUVENILE && c->size >= sp->size_base * 2) {
                    c->stage = STAGE_SUBADULT;
                } else if (c->stage == STAGE_SUBADULT && c->size >= sp->size_base * 3) {
                    c->stage = STAGE_ADULT;
                } else if (c->stage == STAGE_ADULT && c->size >= sp->size_max * 9 / 10) {
                    c->stage = STAGE_GIANT;
                }
                ESP_LOGI(TAG, "Creature %d grew: size=%d stage=%d", i, c->size, c->stage);
            }
        }

        // §4.10 柔性死亡：hunger > 95 持续 30 分钟 → 睡眠态 → 24h 后消失
        if (c->hunger > 95) {
            if (c->state == 0) {
                c->state = 1; // 进入睡眠态
                c->sleep_timer = 30 * 60; // 30分钟观察期
                ESP_LOGW(TAG, "Creature %d entering sleep (starvation)", i);
            } else if (c->state == 1) {
                if (c->sleep_timer > 0) {
                    c->sleep_timer--;
                } else {
                    // 进入死亡倒计时 (24h = 86400s，简化为 1h = 3600s 加速体验)
                    c->state = 2;
                    c->sleep_timer = 3600;
                }
            } else if (c->state == 2) {
                if (c->sleep_timer > 0) {
                    c->sleep_timer--;
                } else {
                    // §4.10: 温柔消失 → 营养 +20
                    ctx->save.env.nutrients += 20;
                    if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
                    // 移除
                    if (i < ctx->save.creature_count - 1) {
                        ctx->save.creatures[i] = ctx->save.creatures[ctx->save.creature_count - 1];
                    }
                    ctx->save.creature_count--;
                    i--;
                    ESP_LOGW(TAG, "Creature died peacefully, nutrients +20");
                }
            }
        } else if (c->state == 1 && c->hunger < 80) {
            // 恢复（玩家喂食后唤醒）
            c->state = 0;
            c->sleep_timer = 0;
        } else if (c->state == 2 && c->hunger < 80) {
            // 死亡倒计时中也可以被救回
            c->state = 0;
            c->sleep_timer = 0;
        }
    }

    ctx->dirty = true;
}

// ============================================================
// §4.11 稳态修复
// ============================================================
static void update_homeostasis(struct game_context *ctx)
{
    // 每 30 秒检查一次稳态
    if (ctx->save.env.total_seconds % 30 != 0) return;

    struct environment *env = &ctx->save.env;

    // §4.11 藻类爆发：algae > 80 → L2 啃食加速（在 forage_l2 中自然发生）
    // 这里只做额外衰减
    if (env->algae_mass > 80) {
        // 统计 L2 数量，加速消耗
        uint8_t l2_count = 0;
        for (int i = 0; i < ctx->save.creature_count; i++) {
            const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
            if (sp && sp->trophic_level == TROPHIC_L2) l2_count++;
        }
        // 每只 L2 额外消耗 1 点藻类
        uint8_t consume = l2_count;
        if (consume > env->algae_mass - 50) consume = env->algae_mass - 50;
        env->algae_mass -= consume;
    }

    // §4.11 鱼太少：离线时小概率"野生访客"漂入
    // 简化：在线时如果生物数 < 3 且有空位，每 5 分钟 20% 概率漂入
    if (ctx->save.creature_count < 3 && ctx->save.creature_count < MAX_CREATURES) {
        if (ctx->save.env.total_seconds % 300 == 0) {
            if ((esp_random() % 100) < 20) {
                // 随机选一个 L2 或 L3 的 common 物种
                uint8_t target_level = ((esp_random() % 2) == 0) ? TROPHIC_L2 : TROPHIC_L3;
                const struct species_def *sp = species_get_random(target_level, RARITY_COMMON);
                if (sp) {
                    struct creature *c = &ctx->save.creatures[ctx->save.creature_count];
                    memset(c, 0, sizeof(*c));
                    c->species_id = sp->id;
                    c->stage = STAGE_JUVENILE;
                    c->size = sp->size_base;
                    c->pos_x = (int8_t)(esp_random() % 120);
                    c->pos_y = (int8_t)(esp_random() % 120);
                    c->hunger = 30;
                    c->mood = 70;
                    c->state = 0;
                    ctx->save.creature_count++;
                    ESP_LOGI(TAG, "Wild visitor: %s drifted in!", sp->name);
                    ctx->dirty = true;
                }
            }
        }
    }

    // §4.11 鱼太多：觅食半径自动放大 + 生育自然停止
    // （生育停止在 engine_try_breed 中通过 creature_count >= MAX_CREATURES 实现）

    // §4.11 营养枯竭：自动开缸内灯
    if (env->nutrients < 10 && env->sunlight < 50) {
        env->sunlight += 5;
        if (env->sunlight > 100) env->sunlight = 100;
    }

    // §4.11 阳光不足 → 自动开缸内灯（消耗微量光合币）
    if (env->sunlight < 20 && !env->is_daytime) {
        env->sunlight += 2;
        if (env->sunlight > 100) env->sunlight = 100;
        // 消耗 1 光合币
        if (ctx->save.photosynth_coins > 0) {
            ctx->save.photosynth_coins--;
        }
    }
}

// ============================================================
// 摇晃效果处理
// ============================================================
static void apply_shake_effect(struct game_context *ctx)
{
    static uint32_t s_imu_check_timer = 0;
    s_imu_check_timer += ENGINE_TICK_MS;
    if (s_imu_check_timer < 100) return;
    s_imu_check_timer = 0;

    shake_level_t shake = hal_imu_detect_shake();
    if (shake == SHAKE_NONE) return;

    enum shake_effect effect = hal_imu_get_shake_effect(shake);
    switch (effect) {
        case SHAKE_EFFECT_FEED:
            // 全体喂食：hunger -10
            for (int i = 0; i < ctx->save.creature_count; i++) {
                if (ctx->save.creatures[i].hunger >= 10) {
                    ctx->save.creatures[i].hunger -= 10;
                } else {
                    ctx->save.creatures[i].hunger = 0;
                }
                // 唤醒睡眠中的生物
                if (ctx->save.creatures[i].state == 1 || ctx->save.creatures[i].state == 2) {
                    ctx->save.creatures[i].state = 0;
                    ctx->save.creatures[i].sleep_timer = 0;
                }
            }
            ESP_LOGI(TAG, "Shake effect: FEED (-10 hunger, wake all)");
            break;

        case SHAKE_EFFECT_OXYGEN:
            {
                uint16_t o2 = ctx->save.env.oxygen + 20;
                if (o2 > 100) o2 = 100;
                ctx->save.env.oxygen = (uint8_t)o2;
                ESP_LOGI(TAG, "Shake effect: OXYGEN (+20)");
            }
            break;

        case SHAKE_EFFECT_SCATTER:
            // L1 生物 20% 死亡
            for (int i = ctx->save.creature_count - 1; i >= 0; i--) {
                const struct species_def *sp = species_get_by_id(ctx->save.creatures[i].species_id);
                if (sp && sp->trophic_level == TROPHIC_L1) {
                    if ((esp_random() % 100) < 20) {
                        ctx->save.env.nutrients += sp->food_value;
                        if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
                        if (i < ctx->save.creature_count - 1) {
                            ctx->save.creatures[i] = ctx->save.creatures[ctx->save.creature_count - 1];
                        }
                        ctx->save.creature_count--;
                        ESP_LOGW(TAG, "Shake scatter: L1 creature died");
                    }
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
        // §4.11: 水循环 → 营养 +10
        ctx->save.env.nutrients += 10;
        if (ctx->save.env.nutrients > 100) ctx->save.env.nutrients = 100;
        ESP_LOGI(TAG, "Water cycle: nutrients +10");
        ctx->dirty = true;
    }
}

// ============================================================
// 主入口：每秒一帧心跳（§4.2）
// ============================================================
void engine_logic_update(struct game_context *ctx)
{
    if (!ctx) return;

    // §4.2 按顺序执行：
    // 1. 环境更新（阳光、营养、氧气）
    update_environment(ctx);
    // 2-6. 生物行为（觅食、成长、衰减/睡眠、死亡）
    update_creatures(ctx);
    // 7. 稳态修复
    update_homeostasis(ctx);
    // 8. 玩家物理交互
    apply_shake_effect(ctx);
    apply_tilt_effect(ctx);
}
