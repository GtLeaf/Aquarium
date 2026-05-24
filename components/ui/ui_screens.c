#include "ui_screens.h"
#include "ui_main.h"
#include "ui_popup.h"
#include "species_data.h"
#include "engine_main.h"
#include "hal_audio.h"
#include "hal_display.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include <math.h>

static const char *TAG = "ui_screens";

/**
 * 滚动容器事件守卫：拦截 PRESSED/FOCUSED/SCROLL_BEGIN/SCROLL_END 事件，
 * 阻止 LVGL 默认的 lv_obj_add/remove_state 执行，
 * 从而避免 refresh_children_style 递归遍历大量子对象导致 WDT。
 * 滚动本身不受影响（由 indev 内部直接操作 scroll offset）。
 */
static void grid_scroll_guard_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED ||
        code == LV_EVENT_PRESS_LOST || code == LV_EVENT_FOCUSED ||
        code == LV_EVENT_DEFOCUSED || code == LV_EVENT_SCROLL_BEGIN ||
        code == LV_EVENT_SCROLL_END) {
        lv_event_stop_processing(e);
        lv_event_stop_bubbling(e);
    }
}

/**
 * 虚拟化滚动回调：根据视口可见区域 show/hide 子对象。
 * 只有在视口范围内（含上下各 1 个卡片的缓冲区）的对象才可见，
 * 大幅减少 LVGL 每帧需要渲染的对象数量。
 */
static void grid_virtualize_cb(lv_event_t *e)
{
    lv_obj_t *grid = lv_event_get_target(e);
    int32_t scroll_y = lv_obj_get_scroll_y(grid);
    int32_t grid_h = lv_obj_get_height(grid);

    // 可见范围: [scroll_y - buffer, scroll_y + grid_h + buffer]
    int32_t vis_top = scroll_y - 160;  // 缓冲区：1个卡片高度
    int32_t vis_bot = scroll_y + grid_h + 160;

    uint32_t cnt = lv_obj_get_child_count(grid);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(grid, i);
        int32_t cy = lv_obj_get_y(child);
        int32_t ch = lv_obj_get_height(child);

        if ((cy + ch) < vis_top || cy > vis_bot) {
            // 不在视口内 — 隐藏
            if (!lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            // 在视口内 — 显示
            if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_remove_flag(child, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static lv_obj_t *g_main_screen = NULL;
static lv_obj_t *g_status_bar = NULL;
static lv_obj_t *g_tank_area = NULL;
static lv_obj_t *g_creature_objs[MAX_CREATURES] = {0};
static lv_obj_t *g_lbl_sun = NULL;
static lv_obj_t *g_lbl_nutrients = NULL;
static lv_obj_t *g_lbl_oxygen = NULL;
static lv_obj_t *g_lbl_coins = NULL;
static lv_obj_t *g_lbl_creature_count = NULL;
static lv_obj_t *g_btn_settings = NULL;
static lv_obj_t *g_btn_collection = NULL;
static lv_obj_t *g_btn_shop = NULL;
static lv_obj_t *g_btn_upgrade = NULL;

// Button callback forward declarations
static void btn_settings_cb(lv_event_t *e);
static void btn_collection_cb(lv_event_t *e);
static void btn_back_cb(lv_event_t *e);
static void btn_shop_cb(lv_event_t *e);
static void btn_upgrade_cb(lv_event_t *e);
static void btn_shop_buy_cb(lv_event_t *e);

// Creature interaction callbacks
static void creature_click_cb(lv_event_t *e);
static void creature_long_press_cb(lv_event_t *e);

// 滑块回调
static void slider_sun_cb(lv_event_t *e);
static void slider_temp_cb(lv_event_t *e);

// 营养级对应颜色（色块代替美术资源）
static lv_color_t trophic_colors[] = {
    LV_COLOR_MAKE(0, 200, 0),    // L1 生产者 - 绿
    LV_COLOR_MAKE(200, 150, 0),  // L2 食藻者 - 橙
    LV_COLOR_MAKE(0, 100, 200),  // L3 中型鱼 - 蓝
    LV_COLOR_MAKE(200, 0, 100),  // L4A 顶级 - 紫红
    LV_COLOR_MAKE(150, 0, 200),  // L4B 中大型 - 紫
};

static lv_color_t get_trophic_color(uint8_t level)
{
    switch (level) {
        case TROPHIC_L1: return trophic_colors[0];
        case TROPHIC_L2: return trophic_colors[1];
        case TROPHIC_L3: return trophic_colors[2];
        case TROPHIC_L4A: return trophic_colors[3];
        case TROPHIC_L4B: return trophic_colors[4];
        default: return lv_color_white();
    }
}

static void create_status_bar(void)
{
    g_status_bar = lv_obj_create(g_main_screen);
    lv_obj_set_size(g_status_bar, 368, 40);
    lv_obj_set_pos(g_status_bar, 0, 0);
    lv_obj_set_style_bg_color(g_status_bar, lv_color_make(20, 30, 40), 0);
    lv_obj_set_style_bg_opa(g_status_bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_status_bar, 0, 0);
    lv_obj_set_style_pad_all(g_status_bar, 4, 0);

    // 阳光
    g_lbl_sun = lv_label_create(g_status_bar);
    lv_obj_set_pos(g_lbl_sun, 20, 4);
    lv_label_set_text(g_lbl_sun, "☀ 60");
    lv_obj_set_style_text_color(g_lbl_sun, lv_color_make(255, 220, 0), 0);
    lv_obj_set_style_text_font(g_lbl_sun, &lv_font_montserrat_14, 0);

    // 营养
    g_lbl_nutrients = lv_label_create(g_status_bar);
    lv_obj_set_pos(g_lbl_nutrients, 80, 4);
    lv_label_set_text(g_lbl_nutrients, "N 40");
    lv_obj_set_style_text_color(g_lbl_nutrients, lv_color_make(0, 200, 100), 0);
    lv_obj_set_style_text_font(g_lbl_nutrients, &lv_font_montserrat_14, 0);

    // 氧气
    g_lbl_oxygen = lv_label_create(g_status_bar);
    lv_obj_set_pos(g_lbl_oxygen, 150, 4);
    lv_label_set_text(g_lbl_oxygen, "O2 80");
    lv_obj_set_style_text_color(g_lbl_oxygen, lv_color_make(100, 200, 255), 0);
    lv_obj_set_style_text_font(g_lbl_oxygen, &lv_font_montserrat_14, 0);

    // 光合币
    g_lbl_coins = lv_label_create(g_status_bar);
    lv_obj_set_pos(g_lbl_coins, 230, 4);
    lv_label_set_text(g_lbl_coins, "$ 0");
    lv_obj_set_style_text_color(g_lbl_coins, lv_color_make(255, 200, 0), 0);
    lv_obj_set_style_text_font(g_lbl_coins, &lv_font_montserrat_14, 0);

    // 生物数
    g_lbl_creature_count = lv_label_create(g_status_bar);
    lv_obj_set_pos(g_lbl_creature_count, 300, 4);
    lv_label_set_text(g_lbl_creature_count, "3/24");
    lv_obj_set_style_text_color(g_lbl_creature_count, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_lbl_creature_count, &lv_font_montserrat_14, 0);
}

static void create_tank_area(void)
{
    g_tank_area = lv_obj_create(g_main_screen);
    lv_obj_set_size(g_tank_area, 368, 360);
    lv_obj_set_pos(g_tank_area, 0, 44);
    lv_obj_set_style_bg_color(g_tank_area, lv_color_make(10, 40, 60), 0);
    lv_obj_set_style_bg_opa(g_tank_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_tank_area, 2, 0);
    lv_obj_set_style_border_color(g_tank_area, lv_color_make(0, 80, 120), 0);
    lv_obj_set_style_pad_all(g_tank_area, 0, 0);
    lv_obj_clear_flag(g_tank_area, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_screen_main_create(void)
{
    g_main_screen = lv_obj_create(NULL);
    lv_obj_set_size(g_main_screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(g_main_screen, lv_color_black(), 0);

    create_status_bar();
    create_tank_area();

    // 设置按钮（右下角）
    g_btn_settings = lv_btn_create(g_main_screen);
    lv_obj_set_size(g_btn_settings, 60, 32);
    lv_obj_set_pos(g_btn_settings, 290, 410);
    lv_obj_set_style_bg_color(g_btn_settings, lv_color_make(60, 60, 60), 0);
    lv_obj_set_style_radius(g_btn_settings, 6, 0);
    lv_obj_t *lbl_set = lv_label_create(g_btn_settings);
    lv_obj_center(lbl_set);
    lv_obj_set_style_text_color(lbl_set, lv_color_white(), 0);
    lv_label_set_text(lbl_set, "Set");

    // 图鉴按钮（左下角）
    g_btn_collection = lv_btn_create(g_main_screen);
    lv_obj_set_size(g_btn_collection, 60, 32);
    lv_obj_set_pos(g_btn_collection, 20, 410);
    lv_obj_set_style_bg_color(g_btn_collection, lv_color_make(60, 60, 60), 0);
    lv_obj_set_style_radius(g_btn_collection, 6, 0);
    lv_obj_t *lbl_col = lv_label_create(g_btn_collection);
    lv_obj_center(lbl_col);
    lv_obj_set_style_text_color(lbl_col, lv_color_white(), 0);
    lv_label_set_text(lbl_col, "Col");
    lv_obj_add_event_cb(g_btn_collection, btn_collection_cb, LV_EVENT_CLICKED, NULL);

    // 商店按钮（底部中间偏左）
    g_btn_shop = lv_btn_create(g_main_screen);
    lv_obj_set_size(g_btn_shop, 60, 32);
    lv_obj_set_pos(g_btn_shop, 100, 410);
    lv_obj_set_style_bg_color(g_btn_shop, lv_color_make(60, 60, 60), 0);
    lv_obj_set_style_radius(g_btn_shop, 6, 0);
    lv_obj_t *lbl_shop = lv_label_create(g_btn_shop);
    lv_obj_center(lbl_shop);
    lv_obj_set_style_text_color(lbl_shop, lv_color_white(), 0);
    lv_label_set_text(lbl_shop, "Shop");
    lv_obj_add_event_cb(g_btn_shop, btn_shop_cb, LV_EVENT_CLICKED, NULL);

    // 升级按钮（底部中间偏右）
    g_btn_upgrade = lv_btn_create(g_main_screen);
    lv_obj_set_size(g_btn_upgrade, 60, 32);
    lv_obj_set_pos(g_btn_upgrade, 190, 410);
    lv_obj_set_style_bg_color(g_btn_upgrade, lv_color_make(60, 60, 60), 0);
    lv_obj_set_style_radius(g_btn_upgrade, 6, 0);
    lv_obj_t *lbl_upg = lv_label_create(g_btn_upgrade);
    lv_obj_center(lbl_upg);
    lv_obj_set_style_text_color(lbl_upg, lv_color_white(), 0);
    lv_label_set_text(lbl_upg, "Upg");
    lv_obj_add_event_cb(g_btn_upgrade, btn_upgrade_cb, LV_EVENT_CLICKED, NULL);

    // 设置按钮添加回调
    lv_obj_add_event_cb(g_btn_settings, btn_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(g_main_screen);
    ESP_LOGI(TAG, "Main screen created");
}

void ui_screen_main_update(const struct game_context *ctx)
{
    if (!ctx || !g_main_screen) return;

    const struct game_save *save = &ctx->save;

    // 状态栏：仅在数值变化时刷新 label
    static uint8_t  prev_sun = 0xFF;
    static uint8_t  prev_nut = 0xFF;
    static uint8_t  prev_o2  = 0xFF;
    static uint32_t prev_coins = 0xFFFFFFFF;
    static uint8_t  prev_count = 0xFF;

    char buf[32];

    if (save->env.sunlight != prev_sun) {
        prev_sun = save->env.sunlight;
        snprintf(buf, sizeof(buf), "☀ %d", prev_sun);
        lv_label_set_text(g_lbl_sun, buf);
    }
    if (save->env.nutrients != prev_nut) {
        prev_nut = save->env.nutrients;
        snprintf(buf, sizeof(buf), "N %d", prev_nut);
        lv_label_set_text(g_lbl_nutrients, buf);
    }
    if (save->env.oxygen != prev_o2) {
        prev_o2 = save->env.oxygen;
        snprintf(buf, sizeof(buf), "O2 %d", prev_o2);
        lv_label_set_text(g_lbl_oxygen, buf);
    }
    if (save->photosynth_coins != prev_coins) {
        prev_coins = save->photosynth_coins;
        snprintf(buf, sizeof(buf), "$ %lu", (unsigned long)prev_coins);
        lv_label_set_text(g_lbl_coins, buf);
    }
    if (save->creature_count != prev_count) {
        prev_count = save->creature_count;
        snprintf(buf, sizeof(buf), "%d/%d", prev_count, MAX_CREATURES);
        lv_label_set_text(g_lbl_creature_count, buf);
    }

    // 生物色块：缓存属性，仅变化时更新 LVGL 对象
    static struct {
        int16_t pos_x, pos_y;
        int16_t size;
        uint8_t trophic_level;
        uint8_t state;
    } creature_cache[MAX_CREATURES];
    static bool cache_initialized = false;
    if (!cache_initialized) {
        memset(creature_cache, 0xFF, sizeof(creature_cache));
        cache_initialized = true;
    }

    for (int i = 0; i < MAX_CREATURES; i++) {
        if (i < save->creature_count) {
            const struct creature *c = &save->creatures[i];
            const struct species_def *sp = species_get_by_id(c->species_id);
            if (!sp) continue;

            lv_obj_t *obj = g_creature_objs[i];
            if (!obj) {
                obj = lv_obj_create(g_tank_area);
                lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_border_width(obj, 1, 0);
                lv_obj_set_style_border_color(obj, lv_color_white(), 0);
                lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_add_event_cb(obj, creature_click_cb, LV_EVENT_CLICKED, NULL);
                lv_obj_add_event_cb(obj, creature_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);
                g_creature_objs[i] = obj;
                // 强制首帧刷新
                creature_cache[i].pos_x = -1;
                creature_cache[i].pos_y = -1;
                creature_cache[i].size = -1;
                creature_cache[i].trophic_level = 0xFF;
                creature_cache[i].state = 0xFF;
            }

            // 位置映射 (0-127 -> 屏幕坐标)
            int16_t screen_x = (c->pos_x * 340) / 127 + 10;
            int16_t screen_y = (c->pos_y * 320) / 127 + 20;

            // 大小根据体型
            int16_t sz = sp->size_base / 3;
            if (sz < 8) sz = 8;
            if (sz > 32) sz = 32;

            if (sz != creature_cache[i].size) {
                lv_obj_set_size(obj, sz, sz);
                creature_cache[i].size = sz;
            }
            if (screen_x != creature_cache[i].pos_x ||
                screen_y != creature_cache[i].pos_y) {
                lv_obj_set_pos(obj, screen_x, screen_y);
                creature_cache[i].pos_x = screen_x;
                creature_cache[i].pos_y = screen_y;
            }
            if (sp->trophic_level != creature_cache[i].trophic_level) {
                lv_obj_set_style_bg_color(obj, get_trophic_color(sp->trophic_level), 0);
                creature_cache[i].trophic_level = sp->trophic_level;
            }

            lv_obj_set_user_data(obj, (void *)(intptr_t)i);

            if (c->state != creature_cache[i].state) {
                creature_cache[i].state = c->state;
                if (c->state == 1) {
                    lv_obj_set_style_bg_opa(obj, LV_OPA_30, 0);
                } else {
                    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
                }
            }
        } else {
            if (g_creature_objs[i]) {
                lv_obj_del(g_creature_objs[i]);
                g_creature_objs[i] = NULL;
            }
        }
    }
}

lv_obj_t* ui_get_main_screen(void)
{
    return g_main_screen;
}

// ========== 设置界面 ==========
static lv_obj_t *g_settings_screen = NULL;
static lv_obj_t *g_settings_back_btn = NULL;

// 按钮事件回调
static void btn_settings_cb(lv_event_t *e)
{
    (void)e;
    ui_navigate_settings();
}

static void btn_collection_cb(lv_event_t *e)
{
    (void)e;
    ui_navigate_collection();
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "btn_back_cb");
    ui_navigate_home();
}

static void slider_sun_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    struct game_context *ctx = engine_get_context();
    if (ctx) {
        ctx->save.env.sunlight = (uint8_t)val;
        engine_mark_dirty();
    }
}

static void slider_temp_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    struct game_context *ctx = engine_get_context();
    if (ctx) {
        ctx->save.env.temperature = (uint8_t)val;
        engine_mark_dirty();
    }
}

static void creature_click_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(obj);
    struct game_context *ctx = engine_get_context();
    if (!ctx || idx < 0 || idx >= ctx->save.creature_count) return;

    struct creature *c = &ctx->save.creatures[idx];
    const struct species_def *sp = species_get_by_id(c->species_id);
    if (!sp) return;

    // 喂食：饥饿 -20，心情 +10
    if (c->hunger >= 20) {
        c->hunger -= 20;
    } else {
        c->hunger = 0;
    }
    if (c->mood <= 90) c->mood += 10;
    engine_mark_dirty();
    hal_audio_play(SOUND_FEED);

    ESP_LOGI(TAG, "Fed creature %d (%s), hunger=%d, mood=%d", idx, sp->name, c->hunger, c->mood);
}

static void creature_long_press_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(obj);
    struct game_context *ctx = engine_get_context();
    if (!ctx || idx < 0 || idx >= ctx->save.creature_count) return;

    struct creature *c = &ctx->save.creatures[idx];
    const struct species_def *sp = species_get_by_id(c->species_id);
    if (!sp) return;

    // 显示生物详情弹窗
    char msg[256];
    const char *stage_name[] = {"Juvenile", "Sub-adult", "Adult", "Giant"};
    snprintf(msg, sizeof(msg),
             "Species: %s\n"
             "Stage: %s\n"
             "Size: %d/%d\n"
             "Hunger: %d\n"
             "Mood: %d\n"
             "Age: %lu sec",
             sp->name,
             stage_name[c->stage],
             c->size, sp->size_max,
             c->hunger,
             c->mood,
             (unsigned long)c->age_seconds);
    ui_popup_show_reward(sp->name, msg);
    hal_audio_play(SOUND_CLICK);
}

static void btn_shop_cb(lv_event_t *e)
{
    (void)e;
    struct game_context *ctx = engine_get_context();
    if (ctx) {
        ESP_LOGI(TAG, "btn_shop_cb: coins=%lu, ctx=%p, save=%p",
                 (unsigned long)ctx->save.photosynth_coins, ctx, &ctx->save);
        ui_screen_shop_show(&ctx->save);
    }
}

static void btn_upgrade_cb(lv_event_t *e)
{
    (void)e;
    struct game_context *ctx = engine_get_context();
    if (ctx) {
        uint32_t cost = engine_get_upgrade_cost(ctx->save.tank_level);
        if (engine_upgrade_tank(&ctx->save)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Tank upgraded to L%d!", ctx->save.tank_level);
            ui_popup_show_reward("Upgrade Success", msg);
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg), "Need %lu coins", (unsigned long)cost);
            ui_popup_show_reward("Upgrade Failed", msg);
        }
    }
}

void ui_screen_settings_create(void)
{
    if (g_settings_screen) return;

    g_settings_screen = lv_obj_create(NULL);
    lv_obj_set_size(g_settings_screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(g_settings_screen, lv_color_make(15, 25, 35), 0);

    // 标题
    lv_obj_t *title = lv_label_create(g_settings_screen);
    lv_obj_set_pos(title, 140, 20);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // 返回按钮
    g_settings_back_btn = lv_btn_create(g_settings_screen);
    lv_obj_set_size(g_settings_back_btn, 80, 36);
    lv_obj_set_pos(g_settings_back_btn, 20, 20);
    lv_obj_set_style_bg_color(g_settings_back_btn, lv_color_make(0, 100, 150), 0);

    lv_obj_t *lbl = lv_label_create(g_settings_back_btn);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_label_set_text(lbl, "< Back");
    lv_obj_add_event_cb(g_settings_back_btn, btn_back_cb, LV_EVENT_CLICKED, NULL);

    // 阳光滑块
    lv_obj_t *sun_label = lv_label_create(g_settings_screen);
    lv_obj_set_pos(sun_label, 20, 80);
    lv_obj_set_style_text_color(sun_label, lv_color_white(), 0);
    lv_label_set_text(sun_label, "Sunlight");

    lv_obj_t *sun_slider = lv_slider_create(g_settings_screen);
    lv_obj_set_size(sun_slider, 200, 20);
    lv_obj_set_pos(sun_slider, 120, 80);
    lv_slider_set_range(sun_slider, 0, 100);
    lv_obj_add_event_cb(sun_slider, slider_sun_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 温度滑块
    lv_obj_t *temp_label = lv_label_create(g_settings_screen);
    lv_obj_set_pos(temp_label, 20, 130);
    lv_obj_set_style_text_color(temp_label, lv_color_white(), 0);
    lv_label_set_text(temp_label, "Temp");

    lv_obj_t *temp_slider = lv_slider_create(g_settings_screen);
    lv_obj_set_size(temp_slider, 200, 20);
    lv_obj_set_pos(temp_slider, 120, 130);
    lv_slider_set_range(temp_slider, 20, 30);
    lv_obj_add_event_cb(temp_slider, slider_temp_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void ui_screen_settings_show(void)
{
    if (!g_settings_screen) ui_screen_settings_create();
    lv_scr_load(g_settings_screen);
}

void ui_screen_settings_hide(void)
{
    // Navigation handled by ui_navigate_home
}

// ========== 图鉴界面 ==========
static lv_obj_t *g_collection_screen = NULL;
lv_obj_t *g_collection_grid = NULL;

void ui_screen_collection_create(void)
{
    if (g_collection_screen) return;

    g_collection_screen = lv_obj_create(NULL);
    lv_obj_set_size(g_collection_screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(g_collection_screen, lv_color_make(15, 25, 35), 0);

    // 标题
    lv_obj_t *title = lv_label_create(g_collection_screen);
    lv_obj_set_pos(title, 140, 20);
    lv_label_set_text(title, "Collection");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(g_collection_screen);
    lv_obj_set_size(back_btn, 80, 36);
    lv_obj_set_pos(back_btn, 20, 20);
    lv_obj_set_style_bg_color(back_btn, lv_color_make(0, 100, 150), 0);

    lv_obj_t *lbl = lv_label_create(back_btn);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_label_set_text(lbl, "< Back");
    lv_obj_add_event_cb(back_btn, btn_back_cb, LV_EVENT_CLICKED, NULL);

    // 物种网格容器
    g_collection_grid = lv_obj_create(g_collection_screen);
    lv_obj_set_size(g_collection_grid, 340, 360);
    lv_obj_set_pos(g_collection_grid, 14, 70);
    lv_obj_set_style_bg_opa(g_collection_grid, LV_OPA_0, 0);
    lv_obj_set_style_border_width(g_collection_grid, 0, 0);
    lv_obj_set_style_shadow_width(g_collection_grid, 0, 0);
    lv_obj_set_style_pad_all(g_collection_grid, 4, 0);
    lv_obj_set_flex_flow(g_collection_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(g_collection_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(g_collection_grid, 8, 0);
    lv_obj_add_flag(g_collection_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_collection_grid, LV_DIR_VER);
    lv_obj_clear_flag(g_collection_grid, LV_OBJ_FLAG_SCROLL_ELASTIC);  // 去掉回弹效果
    lv_obj_add_event_cb(g_collection_grid, grid_scroll_guard_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(g_collection_grid, grid_virtualize_cb, LV_EVENT_SCROLL, NULL);
}

// 分批填充图鉴列表（避免一次性创建过多对象触发 WDT）
#define COLL_BATCH_SIZE 1
static lv_timer_t *s_coll_fill_timer = NULL;
static int         s_coll_fill_index = 0;
static const struct game_save *g_coll_save_ref = NULL;

static void coll_fill_batch_cb(lv_timer_t *t)
{
    (void)t;
    (void)t;

    if (!g_collection_grid) {
        lv_timer_del(t);
        s_coll_fill_timer = NULL;
        return;
    }

    int count = species_get_count();
    int end = s_coll_fill_index + COLL_BATCH_SIZE;
    if (end > count) end = count;

    for (int i = s_coll_fill_index; i < end; i++) {
        const struct species_def *sp = species_get_by_id(i + 1);
        if (!sp) continue;

        bool unlocked = g_coll_save_ref ?
            ((g_coll_save_ref->species_unlocked & (1ULL << i)) != 0) : false;

        lv_obj_t *card = lv_obj_create(g_collection_grid);
        lv_obj_set_size(card, 72, 72);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_bg_color(card,
            unlocked ? get_trophic_color(sp->trophic_level) : lv_color_make(40, 40, 40), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_make(60, 60, 60), 0);
        lv_obj_set_style_shadow_width(card, 0, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        // 名称
        lv_obj_t *name = lv_label_create(card);
        lv_obj_center(name);
        lv_obj_set_width(name, 64);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(name,
            unlocked ? lv_color_white() : lv_color_make(100, 100, 100), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_label_set_text(name, unlocked ? sp->name : "???");
    }

    s_coll_fill_index = end;

    // 每个 batch 结束后立即隐藏超出视口的卡片，防止下一帧渲染 off-screen label
    lv_obj_send_event(g_collection_grid, LV_EVENT_SCROLL, NULL);

    if (s_coll_fill_index >= count) {
        lv_timer_del(t);
        s_coll_fill_timer = NULL;
        // 完成时最终虚拟化（上面已做，此处保留确保一致性）
        lv_obj_send_event(g_collection_grid, LV_EVENT_SCROLL, NULL);
    }
}

void ui_screen_collection_show(const struct game_save *save)
{
    if (!g_collection_screen) ui_screen_collection_create();
    if (!g_collection_grid) return;

    // 清除旧内容
    lv_obj_clean(g_collection_grid);

    // 分批填充物种卡片（避免 WDT）
    s_coll_fill_index = 0;
    g_coll_save_ref = save;

    if (s_coll_fill_timer) {
        lv_timer_del(s_coll_fill_timer);
        s_coll_fill_timer = NULL;
    }
    s_coll_fill_timer = lv_timer_create(coll_fill_batch_cb, 100, NULL);

    lv_scr_load(g_collection_screen);
}

// 停止图鉴填充（导航离开时调用）
void ui_collection_stop_fill(void)
{
    if (s_coll_fill_timer) {
        lv_timer_del(s_coll_fill_timer);
        s_coll_fill_timer = NULL;
    }
}

void ui_screen_collection_hide(void)
{
    // Navigation handled by ui_navigate_home
}

// ========== 标题画面 ==========
static lv_obj_t *g_title_screen = NULL;
static lv_obj_t *g_title_label = NULL;
static lv_obj_t *g_title_start_btn = NULL;
static lv_obj_t *g_title_stats = NULL;
static bool g_title_visible = false;

static void btn_start_cb(lv_event_t *e)
{
    (void)e;
    g_title_visible = false;
    hal_audio_play(SOUND_CLICK);
    if (g_main_screen) {
        lv_scr_load(g_main_screen);
        engine_set_state(STATE_TANK_VIEW);
        ESP_LOGI(TAG, "===== START BUTTON HANDLED =====");
    } else {
        ESP_LOGE(TAG, "g_main_screen is NULL!");
    }
}

static void btn_reset_cb(lv_event_t *e)
{
    (void)e;
    hal_audio_play(SOUND_WARNING);
    struct game_context *ctx = engine_get_context();
    if (ctx) {
        engine_reset_game(&ctx->save);
        ui_popup_show_reward("Reset", "Game data cleared!");
    }
}

void ui_screen_title_create(void)
{
    if (g_title_screen) return;

    g_title_screen = lv_obj_create(NULL);
    lv_obj_set_size(g_title_screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(g_title_screen, lv_color_black(), 0);

    // 游戏标题
    g_title_label = lv_label_create(g_title_screen);
    lv_obj_set_pos(g_title_label, 34, 120);
    lv_obj_set_width(g_title_label, 300);
    lv_obj_set_style_text_color(g_title_label, lv_color_make(0, 255, 0), 0);  // TEST: pure green text
    lv_obj_set_style_text_font(g_title_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(g_title_label, "ECO TANK\nPixel Aquarium");
    lv_label_set_long_mode(g_title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(g_title_label, LV_TEXT_ALIGN_CENTER, 0);

    // 副标题
    lv_obj_t *subtitle = lv_label_create(g_title_screen);
    lv_obj_set_pos(subtitle, 34, 180);
    lv_obj_set_width(subtitle, 300);
    lv_obj_set_style_text_color(subtitle, lv_color_make(100, 160, 180), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_label_set_text(subtitle, "Pixel Eco-Tank Simulator");
    lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0);

    // 装饰色块（模拟像素风生物）
    lv_obj_t *deco1 = lv_obj_create(g_title_screen);
    lv_obj_set_size(deco1, 24, 24);
    lv_obj_set_pos(deco1, 80, 240);
    lv_obj_set_style_radius(deco1, 4, 0);
    lv_obj_set_style_bg_color(deco1, lv_color_make(0, 200, 0), 0);
    lv_obj_set_style_border_width(deco1, 2, 0);
    lv_obj_set_style_border_color(deco1, lv_color_white(), 0);

    lv_obj_t *deco2 = lv_obj_create(g_title_screen);
    lv_obj_set_size(deco2, 20, 20);
    lv_obj_set_pos(deco2, 170, 250);
    lv_obj_set_style_radius(deco2, 4, 0);
    lv_obj_set_style_bg_color(deco2, lv_color_make(200, 150, 0), 0);
    lv_obj_set_style_border_width(deco2, 2, 0);
    lv_obj_set_style_border_color(deco2, lv_color_white(), 0);

    lv_obj_t *deco3 = lv_obj_create(g_title_screen);
    lv_obj_set_size(deco3, 28, 28);
    lv_obj_set_pos(deco3, 250, 235);
    lv_obj_set_style_radius(deco3, 4, 0);
    lv_obj_set_style_bg_color(deco3, lv_color_make(0, 100, 200), 0);
    lv_obj_set_style_border_width(deco3, 2, 0);
    lv_obj_set_style_border_color(deco3, lv_color_white(), 0);

    // 版本号
    lv_obj_t *ver = lv_label_create(g_title_screen);
    lv_obj_set_pos(ver, 280, 420);
    lv_obj_set_style_text_color(ver, lv_color_make(80, 80, 80), 0);
    lv_obj_set_style_text_font(ver, &lv_font_montserrat_14, 0);
    lv_label_set_text(ver, "v0.4");

    // 存档统计信息
    g_title_stats = lv_label_create(g_title_screen);
    lv_obj_set_pos(g_title_stats, 20, 380);
    lv_obj_set_width(g_title_stats, 200);
    lv_obj_set_style_text_color(g_title_stats, lv_color_make(120, 140, 160), 0);
    lv_obj_set_style_text_font(g_title_stats, &lv_font_montserrat_14, 0);
    lv_label_set_text(g_title_stats, "");

    // 开始按钮
    g_title_start_btn = lv_btn_create(g_title_screen);
    lv_obj_set_size(g_title_start_btn, 140, 48);
    lv_obj_set_pos(g_title_start_btn, 114, 320);
    lv_obj_set_style_bg_color(g_title_start_btn, lv_color_make(0, 150, 200), 0);
    lv_obj_set_style_radius(g_title_start_btn, 10, 0);

    lv_obj_t *lbl = lv_label_create(g_title_start_btn);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(lbl, "START");

    lv_obj_add_event_cb(g_title_start_btn, btn_start_cb, LV_EVENT_CLICKED, NULL);

    // 重置按钮（演示用，小按钮放在角落）
    lv_obj_t *reset_btn = lv_btn_create(g_title_screen);
    lv_obj_set_size(reset_btn, 60, 28);
    lv_obj_set_pos(reset_btn, 20, 410);
    lv_obj_set_style_bg_color(reset_btn, lv_color_make(100, 30, 30), 0);
    lv_obj_set_style_radius(reset_btn, 6, 0);
    lv_obj_t *rlbl = lv_label_create(reset_btn);
    lv_obj_center(rlbl);
    lv_obj_set_style_text_color(rlbl, lv_color_make(200, 150, 150), 0);
    lv_obj_set_style_text_font(rlbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(rlbl, "Reset");
    lv_obj_add_event_cb(reset_btn, btn_reset_cb, LV_EVENT_CLICKED, NULL);
}

void ui_screen_title_show(void)
{
    if (!g_title_screen) ui_screen_title_create();
    g_title_visible = true;
    lv_scr_load(g_title_screen);
    lv_refr_now(lv_display_get_default());
}

bool ui_screen_title_is_visible(void)
{
    return g_title_visible;
}

void ui_screen_title_update(const struct game_save *save)
{
    if (!g_title_screen || !g_title_stats || !save) return;

    // 统计已解锁物种数
    int unlocked = 0;
    for (int i = 0; i < MAX_SPECIES; i++) {
        if (save->species_unlocked & (1ULL << i)) unlocked++;
    }

    char buf[64];
    snprintf(buf, sizeof(buf),
             "Days: %lu  Coins: %lu\nSpecies: %d/%d",
             (unsigned long)save->play_days_total,
             (unsigned long)save->photosynth_coins,
             unlocked, MAX_SPECIES);
    lv_label_set_text(g_title_stats, buf);
}

// ========== 桌面伴侣模式（Ambient Mode）==========
static lv_obj_t *g_ambient_screen = NULL;
static lv_obj_t *g_ambient_time = NULL;
static lv_obj_t *g_ambient_battery = NULL;
static lv_obj_t *g_ambient_fish = NULL;
static bool g_ambient_active = false;
static uint32_t g_ambient_frame = 0;

void ui_ambient_enter(void)
{
    if (g_ambient_active) return;
    g_ambient_active = true;
    g_ambient_frame = 0;

    if (!g_ambient_screen) {
        g_ambient_screen = lv_obj_create(NULL);
        lv_obj_set_size(g_ambient_screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_obj_set_style_bg_color(g_ambient_screen, lv_color_black(), 0);

        // 时间显示（左下角）
        g_ambient_time = lv_label_create(g_ambient_screen);
        lv_obj_set_pos(g_ambient_time, 20, 380);
        lv_obj_set_style_text_color(g_ambient_time, lv_color_white(), 0);
        lv_obj_set_style_text_font(g_ambient_time, &lv_font_montserrat_14, 0);

        // 电量（右上角）
        g_ambient_battery = lv_label_create(g_ambient_screen);
        lv_obj_set_pos(g_ambient_battery, 300, 20);
        lv_obj_set_style_text_color(g_ambient_battery, lv_color_make(0, 255, 100), 0);
        lv_obj_set_style_text_font(g_ambient_battery, &lv_font_montserrat_14, 0);
        lv_label_set_text(g_ambient_battery, "75%");

        // 主鱼色块（简化动画）
        g_ambient_fish = lv_obj_create(g_ambient_screen);
        lv_obj_set_size(g_ambient_fish, 40, 40);
        lv_obj_set_style_radius(g_ambient_fish, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(g_ambient_fish, lv_color_make(0, 150, 255), 0);
        lv_obj_set_style_border_width(g_ambient_fish, 0, 0);
    }

    lv_scr_load(g_ambient_screen);
    ESP_LOGI(TAG, "Ambient mode entered");
}

void ui_ambient_exit(void)
{
    if (!g_ambient_active) return;
    g_ambient_active = false;

    if (g_main_screen) {
        lv_scr_load(g_main_screen);
    }
    ESP_LOGI(TAG, "Ambient mode exited");
}

bool ui_ambient_is_active(void)
{
    return g_ambient_active;
}

void ui_ambient_update(void)
{
    if (!g_ambient_active || !g_ambient_screen) return;

    g_ambient_frame++;

    // 每秒更新一次时间（1 FPS）
    if (g_ambient_frame % (1000 / ENGINE_TICK_MS) == 0) { // 每秒更新一次（基于 ENGINE_TICK_MS）
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
        lv_label_set_text(g_ambient_time, buf);
    }

    // 简单游动动画（正弦波）
    if (g_ambient_fish) {
        float t = g_ambient_frame * 0.02f;
        int16_t x = 160 + (int16_t)(100.0f * sinf(t));
        int16_t y = 200 + (int16_t)(30.0f * cosf(t * 0.7f));
        lv_obj_set_pos(g_ambient_fish, x, y);
    }
}

// ========== 商店界面 ==========
static lv_obj_t *g_shop_screen = NULL;
lv_obj_t *g_shop_grid = NULL;
static lv_obj_t *g_shop_coin_lbl = NULL;
static const struct game_save *g_shop_save_ref = NULL;

// 分页模式：每页最多 10 个 item，避免一次性创建过多对象触发 WDT
#define SHOP_PAGE_SIZE 10
static int         s_shop_current_page = 0;
static lv_obj_t   *s_shop_page_lbl = NULL;
static lv_obj_t   *s_shop_prev_btn = NULL;
static lv_obj_t   *s_shop_next_btn = NULL;

// 按钮上方提示文字渐隐动画：opacity 从 255 -> 0
static void shop_tip_fade_anim_cb(void *obj, int32_t val)
{
    lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)val, 0);
}

// 渐隐结束后删除提示 label
static void shop_tip_fade_done_cb(lv_anim_t *a)
{
    lv_obj_t *lbl = (lv_obj_t *)a->var;
    if (lbl) lv_obj_del(lbl);
}

static void shop_btn_show_feedback(lv_obj_t *btn, const char *text, bool success)
{
    // 在按钮上方创建浮动提示文字
    lv_obj_t *card = lv_obj_get_parent(btn);
    if (!card) return;

    lv_obj_t *tip = lv_label_create(card);
    lv_label_set_text(tip, text);
    lv_obj_set_style_text_font(tip, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(tip,
        success ? lv_color_make(0, 230, 100) : lv_color_make(255, 80, 80), 0);
    lv_obj_set_style_text_opa(tip, LV_OPA_COVER, 0);

    // 放在按钮正上方
    lv_coord_t btn_x = lv_obj_get_x(btn);
    lv_coord_t btn_y = lv_obj_get_y(btn);
    lv_obj_set_pos(tip, btn_x + 15, btn_y - 20);

    // 创建渐隐动画：延迟 1s 后开始，1s 内从 255→0，结束后自动删除
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, tip);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&anim, 1000);
    lv_anim_set_delay(&anim, 1000);
    lv_anim_set_exec_cb(&anim, shop_tip_fade_anim_cb);
    lv_anim_set_ready_cb(&anim, shop_tip_fade_done_cb);
    lv_anim_start(&anim);
}

static void btn_shop_buy_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int species_id = (int)(intptr_t)lv_obj_get_user_data(btn);

    struct game_context *ctx = engine_get_context();
    if (!ctx) return;

    const struct species_def *sp = species_get_by_id((uint8_t)species_id);
    if (!sp) return;

    if (!engine_is_species_unlocked(&ctx->save, (uint8_t)species_id)) {
        shop_btn_show_feedback(btn, "Locked", false);
        hal_audio_play(SOUND_WARNING);
        return;
    }

    if (engine_buy_species(&ctx->save, (uint8_t)species_id)) {
        shop_btn_show_feedback(btn, "OK!", true);
        hal_audio_play(SOUND_REWARD);
        // Update coin display
        if (g_shop_coin_lbl) {
            char buf[32];
            snprintf(buf, sizeof(buf), "$ %lu", (unsigned long)ctx->save.photosynth_coins);
            lv_label_set_text(g_shop_coin_lbl, buf);
        }
    } else {
        shop_btn_show_feedback(btn, "Failed", false);
        hal_audio_play(SOUND_WARNING);
    }
}

// 同步填充当前页的卡片（最多 SHOP_PAGE_SIZE 个，不会触发 WDT）
static void shop_fill_page(void)
{
    if (!g_shop_grid) return;

    lv_obj_clean(g_shop_grid);

    int count = species_get_count();
    int start = s_shop_current_page * SHOP_PAGE_SIZE;
    int end = start + SHOP_PAGE_SIZE;
    if (end > count) end = count;

    for (int i = start; i < end; i++) {
        const struct species_def *sp = species_get_by_id(i + 1);
        if (!sp) continue;

        bool unlocked = g_shop_save_ref ?
            ((g_shop_save_ref->species_unlocked & (1ULL << i)) != 0) : false;
        uint32_t price = engine_get_species_price(i + 1);

        // Card container: full width with padding
        lv_obj_t *card = lv_obj_create(g_shop_grid);
        lv_obj_set_size(card, 336, 130);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_bg_color(card,
            unlocked ? lv_color_make(30, 45, 60) : lv_color_make(30, 30, 30), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card,
            unlocked ? lv_color_make(0, 150, 200) : lv_color_make(60, 60, 60), 0);
        lv_obj_set_style_shadow_width(card, 0, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

        // Species name
        lv_obj_t *name = lv_label_create(card);
        lv_obj_set_pos(name, 16, 20);
        lv_obj_set_width(name, 220);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(name,
            unlocked ? lv_color_white() : lv_color_make(100, 100, 100), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_label_set_text(name, unlocked ? sp->name : "???");

        // Price tag
        lv_obj_t *price_lbl = lv_label_create(card);
        lv_obj_set_pos(price_lbl, 16, 52);
        lv_obj_set_style_text_color(price_lbl, lv_color_make(255, 200, 0), 0);
        lv_obj_set_style_text_font(price_lbl, &lv_font_montserrat_14, 0);
        char price_buf[16];
        snprintf(price_buf, sizeof(price_buf), "$%lu", (unsigned long)price);
        lv_label_set_text(price_lbl, unlocked ? price_buf : "Locked");

        // Trophic level color dot
        lv_obj_t *troph_dot = lv_obj_create(card);
        lv_obj_set_size(troph_dot, 16, 16);
        lv_obj_set_pos(troph_dot, 296, 20);
        lv_obj_set_style_radius(troph_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(troph_dot, get_trophic_color(sp->trophic_level), 0);
        lv_obj_set_style_border_width(troph_dot, 0, 0);

        // Buy button
        lv_obj_t *buy_btn = lv_btn_create(card);
        lv_obj_set_size(buy_btn, 80, 40);
        lv_obj_set_pos(buy_btn, 236, 65);
        lv_obj_set_style_bg_color(buy_btn,
            unlocked ? lv_color_make(0, 150, 100) : lv_color_make(80, 80, 80), 0);
        lv_obj_set_style_radius(buy_btn, 6, 0);
        lv_obj_t *blbl = lv_label_create(buy_btn);
        lv_obj_center(blbl);
        lv_obj_set_style_text_color(blbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(blbl, &lv_font_montserrat_14, 0);
        lv_label_set_text(blbl, unlocked ? "Buy" : "Lock");
        if (unlocked) {
            lv_obj_set_user_data(buy_btn, (void *)(intptr_t)sp->id);
            lv_obj_add_event_cb(buy_btn, btn_shop_buy_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    // 更新页码显示
    if (s_shop_page_lbl) {
        int total_pages = (count + SHOP_PAGE_SIZE - 1) / SHOP_PAGE_SIZE;
        char page_buf[24];
        snprintf(page_buf, sizeof(page_buf), "%d / %d", s_shop_current_page + 1, total_pages);
        lv_label_set_text(s_shop_page_lbl, page_buf);
    }

    // 更新翻页按钮状态
    if (s_shop_prev_btn) {
        if (s_shop_current_page <= 0) {
            lv_obj_add_state(s_shop_prev_btn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_opa(s_shop_prev_btn, LV_OPA_50, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_shop_prev_btn, LV_STATE_DISABLED);
        }
    }
    if (s_shop_next_btn) {
        int total_pages = (count + SHOP_PAGE_SIZE - 1) / SHOP_PAGE_SIZE;
        if (s_shop_current_page >= total_pages - 1) {
            lv_obj_add_state(s_shop_next_btn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_opa(s_shop_next_btn, LV_OPA_50, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_shop_next_btn, LV_STATE_DISABLED);
        }
    }
}

static void btn_shop_prev_cb(lv_event_t *e)
{
    (void)e;
    if (s_shop_current_page > 0) {
        s_shop_current_page--;
        shop_fill_page();
    }
}

static void btn_shop_next_cb(lv_event_t *e)
{
    (void)e;
    int count = species_get_count();
    int total_pages = (count + SHOP_PAGE_SIZE - 1) / SHOP_PAGE_SIZE;
    if (s_shop_current_page < total_pages - 1) {
        s_shop_current_page++;
        shop_fill_page();
    }
}

// 停止商店填充（导航离开时调用）— 分页模式无需特殊处理
void ui_shop_stop_fill(void)
{
    // 分页模式不需要停止 timer，保留接口兼容性
}

void ui_screen_shop_create(void)
{
    if (g_shop_screen) return;

    g_shop_screen = lv_obj_create(NULL);
    lv_obj_set_size(g_shop_screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(g_shop_screen, lv_color_make(15, 25, 35), 0);

    // Title
    lv_obj_t *title = lv_label_create(g_shop_screen);
    lv_obj_set_pos(title, 160, 20);
    lv_label_set_text(title, "Shop");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // Back button
    lv_obj_t *back_btn = lv_btn_create(g_shop_screen);
    lv_obj_set_size(back_btn, 80, 36);
    lv_obj_set_pos(back_btn, 20, 14);
    lv_obj_set_style_bg_color(back_btn, lv_color_make(0, 100, 150), 0);
    lv_obj_t *lbl = lv_label_create(back_btn);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_label_set_text(lbl, "< Back");
    lv_obj_add_event_cb(back_btn, btn_back_cb, LV_EVENT_CLICKED, NULL);

    // Coin display
    g_shop_coin_lbl = lv_label_create(g_shop_screen);
    lv_obj_set_pos(g_shop_coin_lbl, 260, 22);
    lv_obj_set_style_text_color(g_shop_coin_lbl, lv_color_make(255, 200, 0), 0);
    lv_obj_set_style_text_font(g_shop_coin_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(g_shop_coin_lbl, "$ 0");

    // Shop grid container (scrollable within page, paginated across pages)
    g_shop_grid = lv_obj_create(g_shop_screen);
    lv_obj_set_size(g_shop_grid, 352, 340);
    lv_obj_set_pos(g_shop_grid, 8, 56);
    lv_obj_set_style_bg_opa(g_shop_grid, LV_OPA_0, 0);
    lv_obj_set_style_border_width(g_shop_grid, 0, 0);
    lv_obj_set_style_shadow_width(g_shop_grid, 0, 0);
    lv_obj_set_style_pad_all(g_shop_grid, 4, 0);
    lv_obj_set_flex_flow(g_shop_grid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_shop_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(g_shop_grid, 8, 0);
    // 分页模式：页内可滚动（每页最多10个item）
    lv_obj_add_flag(g_shop_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_shop_grid, LV_DIR_VER);
    // 防止滚动状态变更触发 refresh_children_style 导致卡死
    lv_obj_add_event_cb(g_shop_grid, grid_scroll_guard_cb, LV_EVENT_ALL, NULL);

    // 底部分页栏
    s_shop_prev_btn = lv_btn_create(g_shop_screen);
    lv_obj_set_size(s_shop_prev_btn, 72, 36);
    lv_obj_set_pos(s_shop_prev_btn, 40, 400);
    lv_obj_set_style_bg_color(s_shop_prev_btn, lv_color_make(60, 80, 100), 0);
    lv_obj_set_style_radius(s_shop_prev_btn, 6, 0);
    lv_obj_t *prev_lbl = lv_label_create(s_shop_prev_btn);
    lv_obj_center(prev_lbl);
    lv_obj_set_style_text_color(prev_lbl, lv_color_white(), 0);
    lv_label_set_text(prev_lbl, "< Prev");
    lv_obj_add_event_cb(s_shop_prev_btn, btn_shop_prev_cb, LV_EVENT_CLICKED, NULL);

    s_shop_page_lbl = lv_label_create(g_shop_screen);
    lv_obj_set_pos(s_shop_page_lbl, 160, 408);
    lv_obj_set_style_text_color(s_shop_page_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_shop_page_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_shop_page_lbl, "1 / 1");

    s_shop_next_btn = lv_btn_create(g_shop_screen);
    lv_obj_set_size(s_shop_next_btn, 72, 36);
    lv_obj_set_pos(s_shop_next_btn, 248, 400);
    lv_obj_set_style_bg_color(s_shop_next_btn, lv_color_make(60, 80, 100), 0);
    lv_obj_set_style_radius(s_shop_next_btn, 6, 0);
    lv_obj_t *next_lbl = lv_label_create(s_shop_next_btn);
    lv_obj_center(next_lbl);
    lv_obj_set_style_text_color(next_lbl, lv_color_white(), 0);
    lv_label_set_text(next_lbl, "Next >");
    lv_obj_add_event_cb(s_shop_next_btn, btn_shop_next_cb, LV_EVENT_CLICKED, NULL);
}

void ui_screen_shop_show(const struct game_save *save)
{
    if (!g_shop_screen) ui_screen_shop_create();

    g_shop_save_ref = save;

    // Update coin display
    if (g_shop_coin_lbl && save) {
        char buf[32];
        snprintf(buf, sizeof(buf), "$ %lu", (unsigned long)save->photosynth_coins);
        lv_label_set_text(g_shop_coin_lbl, buf);
        ESP_LOGI(TAG, "Shop coins display: %lu", (unsigned long)save->photosynth_coins);
    } else {
        ESP_LOGW(TAG, "Shop coin_lbl=%p, save=%p", g_shop_coin_lbl, save);
    }

    // 分页模式：回到第一页并填充
    s_shop_current_page = 0;
    shop_fill_page();

    lv_scr_load(g_shop_screen);
}

void ui_screen_shop_hide(void)
{
    // Navigation handled by ui_navigate_home
}