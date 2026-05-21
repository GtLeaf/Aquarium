#include "species_data.h"
#include <string.h>

// 30 种物种定义 (基于 PRD v0.4 species.csv)
static const struct species_def species_db[MAX_SPECIES] = {
    // L1 生产者 (5种)
    {1,  "螺旋藻",   TROPHIC_L1, RARITY_COMMON,  20, 5,  20, 5,  1,  0},
    {2,  "苔藓",     TROPHIC_L1, RARITY_COMMON,  15, 8,  25, 8,  2,  0},
    {3,  "水草丛",   TROPHIC_L1, RARITY_COMMON,  10, 12, 35, 12, 3,  0},
    {4,  "绿莫斯",   TROPHIC_L1, RARITY_RARE,    8,  10, 30, 10, 2,  0},
    {5,  "海葵",     TROPHIC_L1, RARITY_LIMITED, 5,  15, 40, 15, 4,  0},

    // L2 食藻者 (4种)
    {6,  "黑壳虾",   TROPHIC_L2, RARITY_COMMON,  15, 10, 30, 10, 3,  0},
    {7,  "苹果螺",   TROPHIC_L2, RARITY_COMMON,  12, 12, 25, 12, 2,  0},
    {8,  "水晶虾",   TROPHIC_L2, RARITY_RARE,    8,  8,  20, 8,  2,  0},
    {9,  "黄金米虾", TROPHIC_L2, RARITY_LIMITED, 6,  10, 25, 10, 3,  0},

    // L3 中型鱼 (14种)
    {10, "灯科鱼",   TROPHIC_L3, RARITY_COMMON,  10, 15, 35, 15, 5,  0},
    {11, "孔雀鱼",   TROPHIC_L3, RARITY_COMMON,  10, 18, 40, 18, 5,  0},
    {12, "斗鱼",     TROPHIC_L3, RARITY_COMMON,  8,  20, 45, 20, 6,  0},
    {13, "小丑鱼",   TROPHIC_L3, RARITY_RARE,    6,  15, 35, 15, 5,  0},
    {14, "水母",     TROPHIC_L3, RARITY_RARE,    5,  22, 50, 22, 4,  0},
    {15, "鼠鱼",     TROPHIC_L3, RARITY_COMMON,  8,  16, 38, 16, 5,  0},
    {16, "熊猫鼠",   TROPHIC_L3, RARITY_RARE,    6,  14, 32, 14, 4,  0},
    {17, "红绿灯",   TROPHIC_L3, RARITY_COMMON,  12, 12, 30, 12, 4,  0},
    {18, "斑马鱼",   TROPHIC_L3, RARITY_COMMON,  12, 14, 33, 14, 4,  0},
    {19, "神仙鱼",   TROPHIC_L3, RARITY_RARE,    5,  25, 55, 25, 7,  0},
    {20, "七彩神仙", TROPHIC_L3, RARITY_LIMITED, 4,  28, 60, 28, 8,  0},
    {21, "荷兰凤凰", TROPHIC_L3, RARITY_RARE,    5,  18, 42, 18, 6,  0},
    {22, "金波子",   TROPHIC_L3, RARITY_LIMITED, 4,  20, 45, 20, 6,  0},
    {23, "蓝波子",   TROPHIC_L3, RARITY_LIMITED, 4,  20, 45, 20, 6,  0},

    // L4 大型鱼 (7种)
    {24, "鲶鱼",     TROPHIC_L4B, RARITY_COMMON,  3,  30, 70, 30, 10, 0},
    {25, "银龙幼鱼", TROPHIC_L4B, RARITY_RARE,    2,  35, 80, 35, 12, 0},
    {26, "天使鱼",   TROPHIC_L4A, RARITY_RARE,    2,  40, 90, 40, 15, 0},
    {27, "黑魔鬼",   TROPHIC_L4A, RARITY_LIMITED, 1,  45, 100, 45, 18, 0},
    {28, "极乐鸟鱼", TROPHIC_L4A, RARITY_LONGTAIL, 1, 50, 100, 50, 20, 1}, // 百日守护
    {29, "龙鱼成体", TROPHIC_L4A, RARITY_LONGTAIL, 1, 55, 100, 55, 22, 2}, // 银龙进化
    {30, "虎纹鸭嘴", TROPHIC_L4A, RARITY_LONGTAIL, 1, 60, 100, 60, 25, 4}, // 满级+雷暴
};

// 事件定义 (24种)
static const struct event_def event_db[] = {
    // 礼物事件 (7种)
    {1,  "海鸥来访", "一只海鸥停在缸边，留下了一份礼物", 0, 30, 60, 1, 0},
    {2,  "漂流瓶",   "一个漂流瓶漂到了缸边",             0, 20, 120, 1, 0},
    {3,  "新手周礼包", "欢迎新主人！这是你的第一份礼物",   5, 100, 0, 1, 0},
    {4,  "半月扶持", "继续探索生态缸的奥秘吧",           5, 80, 0, 1, 0},
    {5,  "贝壳礼物", "潮水带来了一个漂亮的贝壳",         0, 15, 180, 1, 0},
    {6,  "海星礼物", "一只海星爬到了缸边",               0, 10, 240, 1, 0},
    {7,  "珊瑚碎片", "一块美丽的珊瑚碎片",               0, 10, 240, 1, 0},

    // 生物事件 (5种)
    {8,  "海风带来", "海风带来了一只迷路的生物",         0, 25, 90, 1, 0},
    {9,  "迷路的鱼", "一条迷路的鱼游进了视野",           0, 20, 120, 1, 0},
    {10, "客人造访", "一位特殊的客人造访了你的生态缸",    0, 15, 180, 1, 0},
    {11, "鱼群迁徙", "一小群鱼路过你的生态缸",            0, 10, 300, 1, 0},
    {12, "深海来客", "一只深海生物浮了上来",              0, 8,  360, 1, 0},

    // 环境事件 (6种)
    {13, "下雨日",   "天空下起了雨，缸里的生物很活跃",    1, 20, 0, 1, 0},
    {14, "雷暴日",   "雷暴天气！生物们有些紧张",          1, 10, 0, 1, 0},
    {15, "雪天",     "下雪了，水温略微下降",              1, 8,  0, 1, 0},
    {16, "营养雨",   "充电触发了营养雨！藻类快速生长",    2, 50, 0, 1, 0},
    {17, "晨光",     "清晨的阳光洒入缸中",                5, 30, 0, 1, 0},
    {18, "日落",     "夕阳的余晖映照水面",                5, 30, 0, 1, 0},

    // 氛围事件 (4种)
    {19, "星夜",     "繁星点点，生物们安静下来",          5, 25, 0, 1, 0},
    {20, "微风轻拂", "一阵微风吹过水面",                  5, 20, 0, 1, 0},
    {21, "月光",     "月光洒在水面上",                    5, 15, 0, 1, 0},
    {22, "彩虹",     "雨后出现了彩虹",                    5, 10, 0, 1, 0},

    // 保底/扶持 (2种)
    {23, "新物种保底", "你发现了新的物种！",              4, 55, 0, 1, 0}, // 新手周 55%
    {24, "半月保底", "你发现了新的物种！",                4, 45, 0, 1, 0}, // D8-D14 45%
};

#define EVENT_COUNT (sizeof(event_db) / sizeof(event_db[0]))

const struct species_def* species_get_by_id(uint8_t id)
{
    for (int i = 0; i < MAX_SPECIES; i++) {
        if (species_db[i].id == id) {
            return &species_db[i];
        }
    }
    return NULL;
}

const struct species_def* species_get_random(uint8_t trophic_level, uint8_t rarity)
{
    for (int i = 0; i < MAX_SPECIES; i++) {
        if (species_db[i].trophic_level == trophic_level &&
            species_db[i].rarity == rarity) {
            return &species_db[i];
        }
    }
    return NULL;
}

uint8_t species_get_count(void)
{
    return MAX_SPECIES;
}

void species_init_database(void)
{
    // 数据库已静态初始化，无需额外操作
}

const struct event_def* event_get_by_id(uint8_t id)
{
    for (int i = 0; i < EVENT_COUNT; i++) {
        if (event_db[i].id == id) {
            return &event_db[i];
        }
    }
    return NULL;
}

uint8_t event_get_count(void)
{
    return EVENT_COUNT;
}
