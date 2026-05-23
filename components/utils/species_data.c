#include "species_data.h"
#include <string.h>

// 30 种物种定义 (基于 PRD v0.4 species.csv)
static const struct species_def species_db[MAX_SPECIES] = {
    // L1 Producers (5)
    {1,  "Spirulina",   TROPHIC_L1, RARITY_COMMON,  20, 5,  20, 5,  1,  0},
    {2,  "Moss",        TROPHIC_L1, RARITY_COMMON,  15, 8,  25, 8,  2,  0},
    {3,  "Water Grass", TROPHIC_L1, RARITY_COMMON,  10, 12, 35, 12, 3,  0},
    {4,  "Green Moss",  TROPHIC_L1, RARITY_RARE,    8,  10, 30, 10, 2,  0},
    {5,  "Sea Anemone", TROPHIC_L1, RARITY_LIMITED, 5,  15, 40, 15, 4,  0},

    // L2 Algae Eaters (4)
    {6,  "Black Shrimp",   TROPHIC_L2, RARITY_COMMON,  15, 10, 30, 10, 3,  0},
    {7,  "Apple Snail",    TROPHIC_L2, RARITY_COMMON,  12, 12, 25, 12, 2,  0},
    {8,  "Crystal Shrimp", TROPHIC_L2, RARITY_RARE,    8,  8,  20, 8,  2,  0},
    {9,  "Golden Shrimp",  TROPHIC_L2, RARITY_LIMITED, 6,  10, 25, 10, 3,  0},

    // L3 Medium Fish (14)
    {10, "Tetra",          TROPHIC_L3, RARITY_COMMON,  10, 15, 35, 15, 5,  0},
    {11, "Guppy",          TROPHIC_L3, RARITY_COMMON,  10, 18, 40, 18, 5,  0},
    {12, "Betta",          TROPHIC_L3, RARITY_COMMON,  8,  20, 45, 20, 6,  0},
    {13, "Clownfish",      TROPHIC_L3, RARITY_RARE,    6,  15, 35, 15, 5,  0},
    {14, "Jellyfish",      TROPHIC_L3, RARITY_RARE,    5,  22, 50, 22, 4,  0},
    {15, "Corydoras",      TROPHIC_L3, RARITY_COMMON,  8,  16, 38, 16, 5,  0},
    {16, "Panda Cory",     TROPHIC_L3, RARITY_RARE,    6,  14, 32, 14, 4,  0},
    {17, "Neon Tetra",     TROPHIC_L3, RARITY_COMMON,  12, 12, 30, 12, 4,  0},
    {18, "Zebra Danio",    TROPHIC_L3, RARITY_COMMON,  12, 14, 33, 14, 4,  0},
    {19, "Angelfish",      TROPHIC_L3, RARITY_RARE,    5,  25, 55, 25, 7,  0},
    {20, "Discus",         TROPHIC_L3, RARITY_LIMITED, 4,  28, 60, 28, 8,  0},
    {21, "Ram Cichlid",    TROPHIC_L3, RARITY_RARE,    5,  18, 42, 18, 6,  0},
    {22, "Golden Ram",     TROPHIC_L3, RARITY_LIMITED, 4,  20, 45, 20, 6,  0},
    {23, "Blue Ram",       TROPHIC_L3, RARITY_LIMITED, 4,  20, 45, 20, 6,  0},

    // L4 Large Fish (7)
    {24, "Catfish",        TROPHIC_L4B, RARITY_COMMON,  3,  30, 70, 30, 10, 0},
    {25, "Silver Arowana", TROPHIC_L4B, RARITY_RARE,    2,  35, 80, 35, 12, 0},
    {26, "Angel Shark",    TROPHIC_L4A, RARITY_RARE,    2,  40, 90, 40, 15, 0},
    {27, "Black Ghost",    TROPHIC_L4A, RARITY_LIMITED, 1,  45, 100, 45, 18, 0},
    {28, "Bird of Paradise", TROPHIC_L4A, RARITY_LONGTAIL, 1, 50, 100, 50, 20, 1}, // 100-day guardian
    {29, "Adult Arowana",  TROPHIC_L4A, RARITY_LONGTAIL, 1, 55, 100, 55, 22, 2}, // Silver dragon evolution
    {30, "Tiger Shovelnose", TROPHIC_L4A, RARITY_LONGTAIL, 1, 60, 100, 60, 25, 4}, // Max level + thunderstorm
};

// 事件定义 (24种)
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
    {23, "New Species Guarantee", "You discovered a new species!", 4, 55, 0, 1, 0}, // Week 1: 55%
    {24, "Half-month Guarantee", "You discovered a new species!", 4, 45, 0, 1, 0}, // D8-D14: 45%
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
