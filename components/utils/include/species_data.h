#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 营养级
#define TROPHIC_L1      1   // 生产者
#define TROPHIC_L2      2   // 食藻者
#define TROPHIC_L3      3   // 食肉者
#define TROPHIC_L4A     41  // 顶级捕食者 (限2条)
#define TROPHIC_L4B     42  // 中大型 (不占配额)

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

// 最大数量
#define MAX_CREATURES       24
#define MAX_SPECIES         30
#define MAX_ACTIVE_EVENTS   3

// 物种定义
struct species_def {
    uint8_t id;
    const char *name;
    uint8_t trophic_level;  // L1/L2/L3/L4A/L4B
    uint8_t rarity;         // common/rare/limited/longtail
    uint8_t max_per_tank;   // 同种上限
    uint8_t size_base;      // 基础体型 (1-100)
    uint8_t size_max;       // 最大体型
    uint8_t food_value;     // 被吃时提供的营养
    uint8_t oxygen_demand;  // 氧气需求
    uint32_t unlock_flags;  // 解锁条件位图
};

// 生物实例
struct creature {
    uint8_t species_id;     // 物种ID
    uint8_t stage;          // 当前阶段
    uint8_t size;           // 当前体型 (1-100)
    int8_t pos_x;           // 位置 X (0-255 映射到屏幕)
    int8_t pos_y;           // 位置 Y
    int8_t vel_x;           // 速度 X
    int8_t vel_y;           // 速度 Y
    uint8_t hunger;         // 饥饿度 (0-100)
    uint8_t growth_pts;     // 成长点
    uint8_t mood;           // 心情 (0-100)
    uint8_t state;          // 状态: 0=正常, 1=睡眠, 2=死亡倒计时
    uint16_t age_seconds;   // 存活时间(秒)
    uint16_t sleep_timer;   // 睡眠倒计时(秒)
    bool facing_right;      // 朝向
};

// 环境状态
struct environment {
    uint8_t sunlight;       // 阳光 (0-100)
    uint8_t nutrients;      // 营养 (0-100)
    uint8_t oxygen;         // 氧气 (0-100)
    uint8_t algae_mass;     // 藻类生物量 (0-100)
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