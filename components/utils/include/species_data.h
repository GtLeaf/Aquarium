#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 营养级
#define TROPHIC_L1      1   // 生产者（植物）
#define TROPHIC_L2      2   // 食藻者（无脊椎）
#define TROPHIC_L3      3   // 中型捕食者
#define TROPHIC_L4A     41  // 顶级捕食者 (限2条)
#define TROPHIC_L4B     42  // 中大型 (不占L4A配额)

// 稀有度
#define RARITY_COMMON   0   // 常见
#define RARITY_RARE     1   // 稀有
#define RARITY_LIMITED  2   // 限定
#define RARITY_LONGTAIL 3   // 长尾

// 生物阶段
#define STAGE_JUVENILE  0   // 幼体
#define STAGE_SUBADULT  1   // 亚成体
#define STAGE_ADULT     2   // 成体
#define STAGE_GIANT     3   // 巨型

// 物种分类
#define CATEGORY_PLANT        0
#define CATEGORY_INVERTEBRATE 1
#define CATEGORY_FISH         2

// 环境偏好
#define PREF_NONE    0
#define PREF_LOW     1
#define PREF_MEDIUM  2
#define PREF_HIGH    3

// 最大数量
#define MAX_CREATURES       24
#define MAX_SPECIES         30
#define MAX_ACTIVE_EVENTS   3
#define MAX_PREY_SLOTS      4

// 物种定义
struct species_def {
    uint8_t id;
    const char *name;
    const char *name_zh;
    uint8_t category;       // CATEGORY_PLANT / INVERTEBRATE / FISH
    uint8_t trophic_level;  // TROPHIC_L1/L2/L3/L4A/L4B
    uint8_t rarity;         // RARITY_*
    uint8_t max_per_tank;   // 同种上限
    uint8_t size_base;      // 基础体型 (1-100)
    uint8_t size_cap;       // 最大体型
    uint8_t grow_factor;    // 成长系数
    uint8_t hunger_rate;    // 饥饿速率 (次/分钟)
    uint8_t mood_base;      // 基础心情
    uint8_t vitality_base;  // 基础活力
    uint8_t eats[MAX_PREY_SLOTS];      // 主食物种ID列表
    uint8_t alt_eats[MAX_PREY_SLOTS];  // 备选食物物种ID列表
    uint8_t produces;       // 产出物 (0=无)
    uint8_t light_pref;     // 光照偏好 PREF_*
    uint8_t oxygen_need;    // 氧气需求等级
    uint8_t nutrient_need;  // 营养需求等级
    uint8_t oxygen_demand;  // 氧气消耗量 (绝对值，用于环境计算)
    uint8_t sprite_stages;  // 精灵动画阶段数
    uint16_t coin_value;    // 光合币价值
    const char *unlock_desc;  // 解锁描述
    const char *codex_desc;   // 图鉴描述
};

// 生物实例
struct creature {
    uint16_t creature_id;   // 唯一生物ID（全局递增）
    uint8_t species_id;     // 物种ID
    uint8_t stage;          // 当前阶段 STAGE_*
    uint8_t size;           // 当前体型 (1-100)
    int8_t pos_x;           // 位置 X (0-120)
    int8_t pos_y;           // 位置 Y (0-120)
    int8_t vel_x;           // 速度 X
    int8_t vel_y;           // 速度 Y
    uint8_t hunger;         // 饥饿度 (0-100)
    uint8_t growth_pts;     // 成长点
    uint8_t mood;           // 心情 (0-100)
    uint8_t state;          // 状态: 0=正常, 1=睡眠, 2=死亡倒计时
    uint8_t rest_frames;    // 停歇倒计时(帧数): >0时静止不动, 0=运动中
    uint8_t decel_frames;   // 减速阶段帧数: >0时正在减速, 0=正常速度
    uint16_t age_seconds;   // 存活时间(秒)
    uint16_t sleep_timer;   // 睡眠/死亡倒计时(秒)
    bool facing_right;      // 朝向
};

// 环境状态
struct environment {
    uint8_t sunlight;       // 阳光 (0-100)
    uint8_t nutrients;      // 营养 (0-100)
    uint8_t oxygen;         // 氧气 (0-100)
    uint8_t algae_mass;     // 藻类生物量快照 (0-100, 只读派生)
    uint8_t temperature;    // 水温 (20-30C)
    bool is_daytime;        // 是否白天
    uint32_t total_seconds; // 总运行时间
};

// 游戏存档
struct game_save {
    uint32_t magic;         // 魔数 'ECOT'
    uint32_t version;       // 版本号
    uint32_t crc32;         // 校验和

    struct environment env;
    struct creature creatures[MAX_CREATURES];
    uint8_t creature_count;

    uint64_t species_unlocked;  // 图鉴位图 (64种)
    uint32_t photosynth_coins;  // 光合币
    uint8_t tank_level;         // 缸等级

    uint32_t play_days_total;   // 总游戏天数
    uint32_t last_save_time;    // 上次保存时间戳
    uint32_t offline_start;     // 离线开始时间

    uint8_t today_events;       // 今日已触发事件数
    uint8_t day_of_year;        // 当前日期 (用于每日重置)

    uint64_t achievements_unlocked; // 成就解锁位图
};

// 事件定义
struct event_def {
    uint8_t id;
    const char *name;
    const char *desc;
    uint8_t trigger_type;   // 0=scheduled, 1=weather, 2=charging, 3=interaction, 4=low_prob, 5=time
    uint8_t rarity;         // 权重
    uint16_t cooldown_min;  // 冷却时间(分钟)
    uint8_t daily_cap;      // 每日上限
    uint32_t precondition;  // 前置条件位图
};

// 获取物种定义
const struct species_def* species_get_by_id(uint8_t id);
const struct species_def* species_get_random(uint8_t trophic_level, uint8_t rarity);
uint8_t species_get_count(void);

// 事件查询
const struct event_def* event_get_by_id(uint8_t id);
uint8_t event_get_count(void);

// 初始化默认物种数据库
void species_init_database(void);

#ifdef __cplusplus
}
#endif
