#include "species_data.h"
#include <string.h>
#include "esp_random.h"

// ============================================================
// 30 种物种定义 (基于 species.csv)
// 物种ID: 1-5=L1植物, 6-9=L2无脊椎, 10-23=L3中型鱼, 24-30=L4大型鱼
// ============================================================
static const struct species_def species_db[MAX_SPECIES] = {
    // ─── L1 生产者 (植物，5种) ───
    [0] = {
        .id = 1, .name = "Spirulina", .name_zh = "螺旋藻",
        .category = CATEGORY_PLANT, .trophic_level = TROPHIC_L1,
        .rarity = RARITY_COMMON, .max_per_tank = 20,
        .size_base = 5, .size_cap = 20, .grow_factor = 3,
        .hunger_rate = 0, .mood_base = 50, .vitality_base = 80,
        .eats = {0}, .alt_eats = {0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = 0, .nutrient_need = PREF_LOW,
        .oxygen_demand = 0, .sprite_stages = 3, .coin_value = 5,
        .unlock_desc = "Default", .codex_desc = "微型藻类，生态缸基石"
    },
    [1] = {
        .id = 2, .name = "Moss Ball", .name_zh = "苔藓球",
        .category = CATEGORY_PLANT, .trophic_level = TROPHIC_L1,
        .rarity = RARITY_COMMON, .max_per_tank = 15,
        .size_base = 8, .size_cap = 25, .grow_factor = 2,
        .hunger_rate = 0, .mood_base = 50, .vitality_base = 90,
        .eats = {0}, .alt_eats = {0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = 0, .nutrient_need = PREF_LOW,
        .oxygen_demand = 0, .sprite_stages = 3, .coin_value = 8,
        .unlock_desc = "Default", .codex_desc = "球形苔藓，耐低光环境"
    },
    [2] = {
        .id = 3, .name = "Water Grass", .name_zh = "水草",
        .category = CATEGORY_PLANT, .trophic_level = TROPHIC_L1,
        .rarity = RARITY_COMMON, .max_per_tank = 10,
        .size_base = 12, .size_cap = 35, .grow_factor = 4,
        .hunger_rate = 0, .mood_base = 50, .vitality_base = 85,
        .eats = {0}, .alt_eats = {0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = 0, .nutrient_need = PREF_MEDIUM,
        .oxygen_demand = 0, .sprite_stages = 4, .coin_value = 12,
        .unlock_desc = "Default", .codex_desc = "常见水草，产氧能力强"
    },
    [3] = {
        .id = 4, .name = "Red Ludwigia", .name_zh = "红丁香",
        .category = CATEGORY_PLANT, .trophic_level = TROPHIC_L1,
        .rarity = RARITY_RARE, .max_per_tank = 8,
        .size_base = 10, .size_cap = 30, .grow_factor = 3,
        .hunger_rate = 0, .mood_base = 50, .vitality_base = 75,
        .eats = {0}, .alt_eats = {0},
        .produces = 0, .light_pref = PREF_HIGH,
        .oxygen_need = 0, .nutrient_need = PREF_HIGH,
        .oxygen_demand = 0, .sprite_stages = 4, .coin_value = 20,
        .unlock_desc = "Unlock at tank level 2", .codex_desc = "高光需求红色水草，观赏性极佳"
    },
    [4] = {
        .id = 5, .name = "Java Fern", .name_zh = "铁皇冠",
        .category = CATEGORY_PLANT, .trophic_level = TROPHIC_L1,
        .rarity = RARITY_RARE, .max_per_tank = 6,
        .size_base = 15, .size_cap = 40, .grow_factor = 2,
        .hunger_rate = 0, .mood_base = 50, .vitality_base = 95,
        .eats = {0}, .alt_eats = {0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = 0, .nutrient_need = PREF_MEDIUM,
        .oxygen_demand = 0, .sprite_stages = 4, .coin_value = 25,
        .unlock_desc = "Unlock at tank level 3", .codex_desc = "耐阴蕨类，生长缓慢但坚韧"
    },

    // ─── L2 食藻者 (无脊椎，4种) ───
    [5] = {
        .id = 6, .name = "Black Shrimp", .name_zh = "黑壳虾",
        .category = CATEGORY_INVERTEBRATE, .trophic_level = TROPHIC_L2,
        .rarity = RARITY_COMMON, .max_per_tank = 15,
        .size_base = 6, .size_cap = 15, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 60, .vitality_base = 70,
        .eats = {1, 2, 3, 0}, .alt_eats = {4, 5, 0, 0},
        .produces = 0, .light_pref = PREF_NONE,
        .oxygen_need = PREF_LOW, .nutrient_need = PREF_LOW,
        .oxygen_demand = 2, .sprite_stages = 3, .coin_value = 10,
        .unlock_desc = "Default", .codex_desc = "勤劳的除藻工，食量惊人"
    },
    [6] = {
        .id = 7, .name = "Apple Snail", .name_zh = "苹果螺",
        .category = CATEGORY_INVERTEBRATE, .trophic_level = TROPHIC_L2,
        .rarity = RARITY_COMMON, .max_per_tank = 12,
        .size_base = 8, .size_cap = 20, .grow_factor = 2,
        .hunger_rate = 3, .mood_base = 65, .vitality_base = 85,
        .eats = {1, 2, 3, 0}, .alt_eats = {4, 5, 0, 0},
        .produces = 0, .light_pref = PREF_NONE,
        .oxygen_need = PREF_LOW, .nutrient_need = PREF_LOW,
        .oxygen_demand = 1, .sprite_stages = 3, .coin_value = 8,
        .unlock_desc = "Default", .codex_desc = "缓慢但高效的清洁工"
    },
    [7] = {
        .id = 8, .name = "Crystal Shrimp", .name_zh = "水晶虾",
        .category = CATEGORY_INVERTEBRATE, .trophic_level = TROPHIC_L2,
        .rarity = RARITY_RARE, .max_per_tank = 8,
        .size_base = 5, .size_cap = 12, .grow_factor = 2,
        .hunger_rate = 3, .mood_base = 55, .vitality_base = 60,
        .eats = {1, 2, 0, 0}, .alt_eats = {3, 0, 0, 0},
        .produces = 0, .light_pref = PREF_NONE,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_LOW,
        .oxygen_demand = 2, .sprite_stages = 3, .coin_value = 30,
        .unlock_desc = "Unlock at tank level 2", .codex_desc = "透明身体的精致虾种，较娇贵"
    },
    [8] = {
        .id = 9, .name = "Bamboo Shrimp", .name_zh = "竹节虾",
        .category = CATEGORY_INVERTEBRATE, .trophic_level = TROPHIC_L2,
        .rarity = RARITY_LIMITED, .max_per_tank = 6,
        .size_base = 10, .size_cap = 25, .grow_factor = 3,
        .hunger_rate = 5, .mood_base = 60, .vitality_base = 75,
        .eats = {1, 2, 3, 4}, .alt_eats = {5, 0, 0, 0},
        .produces = 0, .light_pref = PREF_NONE,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_MEDIUM,
        .oxygen_demand = 3, .sprite_stages = 4, .coin_value = 50,
        .unlock_desc = "Limited event", .codex_desc = "大型滤食虾，食量大但温顺"
    },

    // ─── L3 中型捕食者 (鱼类，14种) ───
    [9] = {
        .id = 10, .name = "Neon Tetra", .name_zh = "霓虹灯鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_COMMON, .max_per_tank = 10,
        .size_base = 8, .size_cap = 20, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 70, .vitality_base = 75,
        .eats = {6, 8, 0, 0}, .alt_eats = {7, 9, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 4, .sprite_stages = 3, .coin_value = 15,
        .unlock_desc = "Default", .codex_desc = "最受欢迎的群游小鱼"
    },
    [10] = {
        .id = 11, .name = "Guppy", .name_zh = "孔雀鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_COMMON, .max_per_tank = 10,
        .size_base = 10, .size_cap = 25, .grow_factor = 4,
        .hunger_rate = 5, .mood_base = 75, .vitality_base = 80,
        .eats = {6, 8, 9, 0}, .alt_eats = {7, 0, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 4, .sprite_stages = 4, .coin_value = 12,
        .unlock_desc = "Default", .codex_desc = "色彩绚丽的万鱼之王"
    },
    [11] = {
        .id = 12, .name = "Betta", .name_zh = "斗鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_COMMON, .max_per_tank = 3,
        .size_base = 12, .size_cap = 30, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 60, .vitality_base = 85,
        .eats = {6, 8, 0, 0}, .alt_eats = {9, 0, 0, 0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = PREF_LOW, .nutrient_need = PREF_NONE,
        .oxygen_demand = 5, .sprite_stages = 4, .coin_value = 20,
        .unlock_desc = "Default", .codex_desc = "华丽的独行侠，领地意识强"
    },
    [12] = {
        .id = 13, .name = "Corydoras", .name_zh = "鼠鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_COMMON, .max_per_tank = 8,
        .size_base = 10, .size_cap = 22, .grow_factor = 2,
        .hunger_rate = 3, .mood_base = 75, .vitality_base = 80,
        .eats = {6, 7, 0, 0}, .alt_eats = {8, 9, 0, 0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 4, .sprite_stages = 3, .coin_value = 15,
        .unlock_desc = "Default", .codex_desc = "底层清洁小能手"
    },
    [13] = {
        .id = 14, .name = "Zebra Danio", .name_zh = "斑马鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_COMMON, .max_per_tank = 12,
        .size_base = 8, .size_cap = 18, .grow_factor = 4,
        .hunger_rate = 5, .mood_base = 80, .vitality_base = 90,
        .eats = {6, 8, 0, 0}, .alt_eats = {7, 0, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 3, .sprite_stages = 3, .coin_value = 10,
        .unlock_desc = "Default", .codex_desc = "活泼好动的条纹小鱼"
    },
    [14] = {
        .id = 15, .name = "Clownfish", .name_zh = "小丑鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_RARE, .max_per_tank = 4,
        .size_base = 12, .size_cap = 28, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 70, .vitality_base = 75,
        .eats = {6, 9, 0, 0}, .alt_eats = {7, 8, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 5, .sprite_stages = 4, .coin_value = 35,
        .unlock_desc = "Unlock at tank level 2", .codex_desc = "橙白相间的海洋明星"
    },
    [15] = {
        .id = 16, .name = "Angelfish", .name_zh = "神仙鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_RARE, .max_per_tank = 4,
        .size_base = 15, .size_cap = 40, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 65, .vitality_base = 70,
        .eats = {6, 8, 9, 0}, .alt_eats = {7, 0, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_HIGH, .nutrient_need = PREF_NONE,
        .oxygen_demand = 7, .sprite_stages = 4, .coin_value = 45,
        .unlock_desc = "Unlock at tank level 3", .codex_desc = "优雅的三角帆形热带鱼"
    },
    [16] = {
        .id = 17, .name = "Discus", .name_zh = "七彩神仙",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_LIMITED, .max_per_tank = 3,
        .size_base = 18, .size_cap = 45, .grow_factor = 2,
        .hunger_rate = 3, .mood_base = 55, .vitality_base = 60,
        .eats = {6, 8, 0, 0}, .alt_eats = {9, 0, 0, 0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = PREF_HIGH, .nutrient_need = PREF_NONE,
        .oxygen_demand = 8, .sprite_stages = 4, .coin_value = 80,
        .unlock_desc = "Limited event", .codex_desc = "热带鱼之王，对水质敏感"
    },
    [17] = {
        .id = 18, .name = "Ram Cichlid", .name_zh = "荷兰凤凰",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_RARE, .max_per_tank = 4,
        .size_base = 12, .size_cap = 28, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 65, .vitality_base = 70,
        .eats = {6, 8, 0, 0}, .alt_eats = {7, 9, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 5, .sprite_stages = 4, .coin_value = 40,
        .unlock_desc = "Unlock at tank level 3", .codex_desc = "色彩斑斓的小型慈鲷"
    },
    [18] = {
        .id = 19, .name = "Jellyfish", .name_zh = "水母",
        .category = CATEGORY_INVERTEBRATE, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_RARE, .max_per_tank = 3,
        .size_base = 14, .size_cap = 35, .grow_factor = 2,
        .hunger_rate = 2, .mood_base = 50, .vitality_base = 65,
        .eats = {6, 8, 9, 0}, .alt_eats = {7, 0, 0, 0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = PREF_LOW, .nutrient_need = PREF_NONE,
        .oxygen_demand = 3, .sprite_stages = 4, .coin_value = 55,
        .unlock_desc = "Unlock at tank level 4", .codex_desc = "半透明的飘逸存在"
    },
    [19] = {
        .id = 20, .name = "Panda Cory", .name_zh = "熊猫鼠",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_RARE, .max_per_tank = 6,
        .size_base = 8, .size_cap = 18, .grow_factor = 2,
        .hunger_rate = 3, .mood_base = 80, .vitality_base = 75,
        .eats = {6, 7, 0, 0}, .alt_eats = {8, 0, 0, 0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 3, .sprite_stages = 3, .coin_value = 25,
        .unlock_desc = "Unlock at tank level 2", .codex_desc = "萌系底栖鼠鱼"
    },
    [20] = {
        .id = 21, .name = "Golden Ram", .name_zh = "金波子",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_LIMITED, .max_per_tank = 3,
        .size_base = 12, .size_cap = 28, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 60, .vitality_base = 65,
        .eats = {6, 8, 0, 0}, .alt_eats = {9, 0, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 5, .sprite_stages = 4, .coin_value = 60,
        .unlock_desc = "Limited event", .codex_desc = "金色变种凤凰鱼"
    },
    [21] = {
        .id = 22, .name = "Blue Ram", .name_zh = "蓝波子",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_LIMITED, .max_per_tank = 3,
        .size_base = 12, .size_cap = 28, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 60, .vitality_base = 65,
        .eats = {6, 8, 0, 0}, .alt_eats = {9, 0, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 5, .sprite_stages = 4, .coin_value = 60,
        .unlock_desc = "Limited event", .codex_desc = "蓝色变种凤凰鱼"
    },
    [22] = {
        .id = 23, .name = "Kuhli Loach", .name_zh = "九间鳅",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L3,
        .rarity = RARITY_COMMON, .max_per_tank = 6,
        .size_base = 10, .size_cap = 24, .grow_factor = 3,
        .hunger_rate = 3, .mood_base = 60, .vitality_base = 80,
        .eats = {6, 7, 0, 0}, .alt_eats = {8, 9, 0, 0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = PREF_LOW, .nutrient_need = PREF_NONE,
        .oxygen_demand = 3, .sprite_stages = 3, .coin_value = 15,
        .unlock_desc = "Default", .codex_desc = "夜行性底层鳅鱼"
    },

    // ─── L4 大型捕食者 (7种) ───
    [23] = {
        .id = 24, .name = "Catfish", .name_zh = "大胡子鲶",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L4B,
        .rarity = RARITY_COMMON, .max_per_tank = 3,
        .size_base = 25, .size_cap = 60, .grow_factor = 3,
        .hunger_rate = 3, .mood_base = 55, .vitality_base = 85,
        .eats = {10, 11, 14, 0}, .alt_eats = {6, 7, 13, 0},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = PREF_MEDIUM, .nutrient_need = PREF_NONE,
        .oxygen_demand = 10, .sprite_stages = 4, .coin_value = 50,
        .unlock_desc = "Unlock at tank level 3", .codex_desc = "夜间活跃的底层霸主"
    },
    [24] = {
        .id = 25, .name = "Silver Arowana", .name_zh = "银龙鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L4B,
        .rarity = RARITY_RARE, .max_per_tank = 2,
        .size_base = 30, .size_cap = 75, .grow_factor = 3,
        .hunger_rate = 4, .mood_base = 50, .vitality_base = 80,
        .eats = {10, 11, 12, 14}, .alt_eats = {13, 15, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_HIGH, .nutrient_need = PREF_NONE,
        .oxygen_demand = 12, .sprite_stages = 4, .coin_value = 100,
        .unlock_desc = "Unlock at tank level 4", .codex_desc = "优雅的古代鱼，跳跃能手"
    },
    [25] = {
        .id = 26, .name = "Oscar", .name_zh = "地图鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L4A,
        .rarity = RARITY_RARE, .max_per_tank = 2,
        .size_base = 28, .size_cap = 65, .grow_factor = 4,
        .hunger_rate = 5, .mood_base = 60, .vitality_base = 85,
        .eats = {10, 11, 13, 14}, .alt_eats = {6, 12, 0, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_HIGH, .nutrient_need = PREF_NONE,
        .oxygen_demand = 12, .sprite_stages = 4, .coin_value = 90,
        .unlock_desc = "Unlock at tank level 4", .codex_desc = "智商最高的观赏鱼之一"
    },
    [26] = {
        .id = 27, .name = "Flowerhorn", .name_zh = "罗汉鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L4A,
        .rarity = RARITY_LIMITED, .max_per_tank = 1,
        .size_base = 30, .size_cap = 70, .grow_factor = 3,
        .hunger_rate = 5, .mood_base = 45, .vitality_base = 90,
        .eats = {10, 11, 12, 13}, .alt_eats = {14, 15, 6, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_HIGH, .nutrient_need = PREF_NONE,
        .oxygen_demand = 15, .sprite_stages = 4, .coin_value = 150,
        .unlock_desc = "Limited event", .codex_desc = "霸气侧漏的风水鱼"
    },
    [27] = {
        .id = 28, .name = "Red Arowana", .name_zh = "红龙鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L4A,
        .rarity = RARITY_LONGTAIL, .max_per_tank = 1,
        .size_base = 35, .size_cap = 85, .grow_factor = 2,
        .hunger_rate = 3, .mood_base = 50, .vitality_base = 80,
        .eats = {10, 11, 12, 14}, .alt_eats = {13, 15, 16, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_HIGH, .nutrient_need = PREF_NONE,
        .oxygen_demand = 18, .sprite_stages = 4, .coin_value = 300,
        .unlock_desc = "100-day guardian reward", .codex_desc = "龙鱼之王，百日守护奖励"
    },
    [28] = {
        .id = 29, .name = "Golden Arowana", .name_zh = "金龙鱼",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L4A,
        .rarity = RARITY_LONGTAIL, .max_per_tank = 1,
        .size_base = 35, .size_cap = 85, .grow_factor = 2,
        .hunger_rate = 3, .mood_base = 50, .vitality_base = 80,
        .eats = {10, 11, 12, 14}, .alt_eats = {13, 15, 16, 0},
        .produces = 0, .light_pref = PREF_MEDIUM,
        .oxygen_need = PREF_HIGH, .nutrient_need = PREF_NONE,
        .oxygen_demand = 20, .sprite_stages = 4, .coin_value = 350,
        .unlock_desc = "Silver Arowana evolution", .codex_desc = "银龙进化体，金鳞闪耀"
    },
    [29] = {
        .id = 30, .name = "Tiger Shovelnose", .name_zh = "虎皮鸭嘴",
        .category = CATEGORY_FISH, .trophic_level = TROPHIC_L4A,
        .rarity = RARITY_LONGTAIL, .max_per_tank = 1,
        .size_base = 40, .size_cap = 100, .grow_factor = 3,
        .hunger_rate = 6, .mood_base = 45, .vitality_base = 90,
        .eats = {10, 11, 12, 13}, .alt_eats = {14, 15, 16, 24},
        .produces = 0, .light_pref = PREF_LOW,
        .oxygen_need = PREF_HIGH, .nutrient_need = PREF_NONE,
        .oxygen_demand = 25, .sprite_stages = 4, .coin_value = 500,
        .unlock_desc = "Max level + thunderstorm event", .codex_desc = "终极掠食者，缸内食物链顶端"
    },
};

// ============================================================
// 事件定义 (24种)
// ============================================================
static const struct event_def event_db[] = {
    // Gift events (7)
    {1,  "Seagull Visit", "A seagull stopped by and left a gift", 0, 30, 60, 1, 0},
    {2,  "Message Bottle", "A message bottle drifted to the tank", 0, 20, 120, 1, 0},
    {3,  "Newbie Pack", "Welcome! Here is your first gift", 5, 100, 0, 1, 0},
    {4,  "Half-month Support", "Keep exploring the eco tank!", 5, 80, 0, 1, 0},
    {5,  "Shell Gift", "The tide brought a beautiful shell", 0, 15, 180, 1, 0},
    {6,  "Starfish Gift", "A starfish crawled to the tank", 0, 10, 240, 1, 0},
    {7,  "Coral Fragment", "A beautiful piece of coral", 0, 10, 240, 1, 0},

    // Creature events (5)
    {8,  "Sea Breeze", "The sea breeze brought a lost creature", 0, 25, 90, 1, 0},
    {9,  "Lost Fish", "A lost fish swam into view", 0, 20, 120, 1, 0},
    {10, "Guest Visit", "A special guest visited your tank", 0, 15, 180, 1, 0},
    {11, "Fish Migration", "A small school of fish passed by", 0, 10, 300, 1, 0},
    {12, "Deep Sea Visitor", "A deep sea creature floated up", 0, 8,  360, 1, 0},

    // Environment events (6)
    {13, "Rainy Day", "It's raining, creatures are active", 1, 20, 0, 1, 0},
    {14, "Thunderstorm", "Thunderstorm! Creatures are nervous", 1, 10, 0, 1, 0},
    {15, "Snow Day", "It's snowing, water temp slightly drops", 1, 8,  0, 1, 0},
    {16, "Nutrient Rain", "Charging triggered nutrient rain! Algae grows fast", 2, 50, 0, 1, 0},
    {17, "Morning Light", "Morning sunlight shines into the tank", 5, 30, 0, 1, 0},
    {18, "Sunset", "Sunset glow reflects on the water", 5, 30, 0, 1, 0},

    // Ambient events (4)
    {19, "Starry Night", "Stars twinkle, creatures calm down", 5, 25, 0, 1, 0},
    {20, "Gentle Breeze", "A gentle breeze blows over the water", 5, 20, 0, 1, 0},
    {21, "Moonlight", "Moonlight shines on the water surface", 5, 15, 0, 1, 0},
    {22, "Rainbow", "A rainbow appeared after the rain", 5, 10, 0, 1, 0},

    // Guarantee events (2)
    {23, "New Species Guarantee", "You discovered a new species!", 4, 55, 0, 1, 0},
    {24, "Half-month Guarantee", "You discovered a new species!", 4, 45, 0, 1, 0},
};

#define EVENT_COUNT (sizeof(event_db) / sizeof(event_db[0]))

// ============================================================
// API 实现
// ============================================================

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
    // 收集匹配的候选
    uint8_t candidates[MAX_SPECIES];
    uint8_t count = 0;
    for (int i = 0; i < MAX_SPECIES; i++) {
        if (species_db[i].id == 0) continue;
        if (species_db[i].trophic_level == trophic_level &&
            species_db[i].rarity == rarity) {
            candidates[count++] = i;
        }
    }
    if (count == 0) return NULL;

    // 随机选一个
    uint8_t pick = esp_random() % count;
    return &species_db[candidates[pick]];
}

uint8_t species_get_count(void)
{
    return MAX_SPECIES;
}

const struct species_def* species_get_by_index(uint8_t index)
{
    if (index >= MAX_SPECIES) return NULL;
    return &species_db[index];
}

void species_init_database(void)
{
    // 数据库已静态初始化，无需额外操作
}

const struct event_def* event_get_by_id(uint8_t id)
{
    for (int i = 0; i < (int)EVENT_COUNT; i++) {
        if (event_db[i].id == id) {
            return &event_db[i];
        }
    }
    return NULL;
}

uint8_t event_get_count(void)
{
    return (uint8_t)EVENT_COUNT;
}
