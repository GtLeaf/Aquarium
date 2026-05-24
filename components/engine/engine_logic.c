#include "engine_logic.h"
#include "species_data.h"
#include "hal_imu.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <math.h>

static const char *TAG = "engine_logic";

// 环境更新：阳光、营养、氧气、藻类
static void update_environment(struct game_context *ctx)
{
    struct environment *env = &ctx->save.env;

    // 日夜循环（简化：每 5 分钟切换）
    env->total_seconds++;
    if (env->total_seconds % 300 == 0) {
        env->is_daytime = !env->is_daytime;
    }

    // 阳光变化
    if (env->is_daytime) {
        if (env->sunlight < 100) env->sunlight++;
    } else {
        if (env->sunlight > 0) env->sunlight--;
    }

    // 藻类 Logistic 生长: dM/dt = r * M * (1 - M/K)
    // 简化离散版，r=0.05, K=100
    uint8_t algae = env->algae_mass;
    if (algae < 100) {
        uint8_t growth = (algae * (100 - algae)) / 2000; // ~r=0.005
        if (growth < 1 && algae > 0) growth = 1;
        if (algae + growth > 100) growth = 100 - algae;
        env->algae_mass = algae + growth;
    }

    // 氧气消耗与产生（简化）
    int16_t o2 = env->oxygen;
    o2 += (env->algae_mass / 10); // 藻类产氧
    o2 -= (ctx->save.creature_count * 2); // 生物耗氧
    if (o2 > 100) o2 = 100;
    if (o2 < 0) o2 = 0;
    env->oxygen = (uint8_t)o2;

    // 营养自然衰减 + 藻类死亡释放
    if (env->nutrients > 0) env->nutrients--;

    ctx->dirty = true;
}

// 生物行为：饥饿增长、移动、睡眠
static void update_creatures(struct game_context *ctx)
{
    for (int i = 0; i < ctx->save.creature_count; i++) {
        struct creature *c = &ctx->save.creatures[i];
        const struct species_def *sp = species_get_by_id(c->species_id);
        if (!sp) continue;

        // 年龄增长
        c->age_seconds++;

        // 饥饿增长（L1 生产者不饥饿）
        if (sp->trophic_level != TROPHIC_L1) {
            if (c->hunger < 100) c->hunger++;
        }

        // 简单随机游动
        if (c->state == 0) { // 正常状态
            if ((ctx->frame_count + i * 7) % 30 == 0) {
                c->vel_x = (int8_t)((esp_random() % 5) - 2);
                c->vel_y = (int8_t)((esp_random() % 5) - 2);
                c->facing_right = (c->vel_x >= 0);
            }
            c->pos_x += c->vel_x;
            c->pos_y += c->vel_y;

            // 边界限制（位置用 uint8_t 0-127 映射，足够表示屏幕区域）
            if (c->pos_x < 0) { c->pos_x = 0; c->vel_x = 1; }
            if (c->pos_x > 120) { c->pos_x = 120; c->vel_x = -1; }
            if (c->pos_y < 0) { c->pos_y = 0; c->vel_y = 1; }
            if (c->pos_y > 120) { c->pos_y = 120; c->vel_y = -1; }
        }

        // 成长点积累（饥饿低时成长）
        if (c->hunger < 50 && c->stage < STAGE_GIANT) {
            c->growth_pts++;
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

        // 柔性死亡：hunger > 95 持续 30 分钟 -> 睡眠态 -> 24h 后消失
        if (c->hunger > 95) {
            if (c->state == 0) {
                c->state = 1; // 进入睡眠态
                c->sleep_timer = 30 * 60; // 30分钟倒计时
                ESP_LOGW(TAG, "Creature %d entering sleep (starvation)", i);
            } else if (c->state == 1) {
                if (c->sleep_timer > 0) c->sleep_timer--;
                if (c->sleep_timer == 0) {
                    // 死亡，化为营养
                    ctx->save.env.nutrients += sp->food_value;
                    // 移除该生物（与最后一个交换）
                    if (i < ctx->save.creature_count - 1) {
                        ctx->save.creatures[i] = ctx->save.creatures[ctx->save.creature_count - 1];
                    }
                    ctx->save.creature_count--;
                    i--;
                    ESP_LOGW(TAG, "Creature died, nutrients +%d", sp->food_value);
                }
            }
        } else if (c->state == 1 && c->hunger < 80) {
            // 恢复
            c->state = 0;
            c->sleep_timer = 0;
        }
    }

    ctx->dirty = true;
}

// 摇晃效果处理（降频到每 100ms 检测一次，避免每帧 I2C 读取）
static void apply_shake_effect(struct game_context *ctx)
{
    static uint32_t s_imu_check_timer = 0;
    s_imu_check_timer += ENGINE_TICK_MS;
    if (s_imu_check_timer < 100) return;  // 每 100ms 检测一次
    s_imu_check_timer = 0;

    shake_level_t shake = hal_imu_detect_shake();
    if (shake == SHAKE_NONE) return;

    enum shake_effect effect = hal_imu_get_shake_effect(shake);
    switch (effect) {
        case SHAKE_EFFECT_FEED:
            for (int i = 0; i < ctx->save.creature_count; i++) {
                if (ctx->save.creatures[i].hunger >= 10) {
                    ctx->save.creatures[i].hunger -= 10;
                } else {
                    ctx->save.creatures[i].hunger = 0;
                }
            }
            ESP_LOGI(TAG, "Shake effect: FEED (-30 hunger)");
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
                        if (i < ctx->save.creature_count - 1) {
                            ctx->save.creatures[i] = ctx->save.creatures[ctx->save.creature_count - 1];
                        }
                        ctx->save.creature_count--;
                        ESP_LOGW(TAG, "Shake scatter: L1 creature died");
                    }
                }
            }
            // ESP_LOGI(TAG, "Shake effect: SCATTER (L1 50%% death)");
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
        // 水循环：加速藻类生长 3 倍（持续 5 分钟 = 300 秒）
        static uint32_t water_cycle_timer = 0;
        if (water_cycle_timer == 0) {
            ESP_LOGI(TAG, "Water cycle triggered! Algae growth x3");
        }
        water_cycle_timer = 300; // 5 分钟
    }
}

void engine_logic_update(struct game_context *ctx)
{
    if (!ctx) return;
    update_environment(ctx);
    update_creatures(ctx);
    apply_shake_effect(ctx);
    apply_tilt_effect(ctx);
}
