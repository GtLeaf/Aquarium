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
    lv_obj_set_pos(g_lbl_sun, 4, 4);
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
    lv_obj_set_pos(g_lbl_creature_count, 310, 4);
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

    // 更新状态栏
    char buf[32];
    snprintf(buf, sizeof(buf), "☀ %d", save->env.sunlight);
    lv_label_set_text(g_lbl_sun, buf);

    snprintf(buf, sizeof(buf), "N %d", save->env.nutrients);
    lv_label_set_text(g_lbl_nutrients, buf);

    snprintf(buf, sizeof(buf), "O2 %d", save->env.oxygen);
    lv_label_set_text(g_lbl_oxygen, buf);

    snprintf(buf, sizeof(buf), "$ %lu", (unsigned long)save->photosynth_coins);
    lv_label_set_text(g_lbl_coins, buf);

    snprintf(buf, sizeof(buf), "%d/%d", save->creature_count, MAX_CREATURES);
    lv_label_set_text(g_lbl_creature_count, buf);

    // 更新或创建生物色块
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
                g_creature_objs[i] = obj;
            }

            // 位置映射 (0-127 -> 屏幕坐标)
            int16_t screen_x = (c->pos_x * 340) / 127 + 10;
            int16_t screen_y = (c->pos_y * 320) / 127 + 20;

            // 大小根据体型
            int16_t sz = sp->size_base / 3;
            if (sz < 8) sz = 8;
            if (sz > 32) sz = 32;

            lv_obj_set_size(obj, sz, sz);
            lv_obj_set_pos(obj, screen_x, screen_y);
            lv_obj_set_style_bg_color(obj, get_trophic_color(sp->trophic_level), 0);

            // 存储索引到 user_data，用于点击识别
            lv_obj_set_user_data(obj, (void *)(intptr_t)i);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(obj, creature_click_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(obj, creature_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);

            // 睡眠态显示半透明
            if (c->state == 1) {
                lv_obj_set_style_bg_opa(obj, LV_OPA_30, 0);
            } else {
                lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
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
    ui_on_interaction();
    ui_navigate_settings();
}

static void btn_collection_cb(lv_event_t *e)
{
    (void)e;
    ui_on_interaction();
    ui_navigate_collection();
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "btn_back_cb");
    ui_on_interaction();
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

    ui_on_interaction();

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

    ui_on_interaction();

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
    ui_on_interaction();
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
    ui_on_interaction();
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
    lv_obj_set_flex_flow(g_collection_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(g_collection_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(g_collection_grid, 8, 0);
    lv_obj_add_flag(g_collection_grid, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_screen_collection_show(const struct game_save *save)
{
    if (!g_collection_screen) ui_screen_collection_create();
    if (!g_collection_grid) return;

    // 清除旧内容
    lv_obj_clean(g_collection_grid);

    // 填充物种卡片
    for (int i = 0; i < species_get_count(); i++) {
        const struct species_def *sp = species_get_by_id(i + 1);
        if (!sp) continue;

        bool unlocked = save ? ((save->species_unlocked & (1ULL << i)) != 0) : false;

        lv_obj_t *card = lv_obj_create(g_collection_grid);
        lv_obj_set_size(card, 72, 72);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_bg_color(card,
            unlocked ? get_trophic_color(sp->trophic_level) : lv_color_make(40, 40, 40), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_make(60, 60, 60), 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

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

    lv_scr_load(g_collection_screen);
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
    ui_on_interaction();
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
    ui_on_interaction();
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
    if (g_ambient_frame % 60 == 0) { // 假设 60 FPS 调用，实际由调用方控制频率
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
static lv_obj_t *g_shop_page_lbl = NULL;
static uint8_t g_shop_page = 0;
#define SHOP_ITEMS_PER_PAGE 6
#define SHOP_TOTAL_PAGES ((MAX_SPECIES + SHOP_ITEMS_PER_PAGE - 1) / SHOP_ITEMS_PER_PAGE)
static const struct game_save *g_shop_save_ref = NULL;  // 临时引用，用于翻页

// 物种价格统一从 engine 获取

static void btn_shop_buy_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int species_id = (int)(intptr_t)lv_obj_get_user_data(btn);

    struct game_context *ctx = engine_get_context();
    if (!ctx) return;

    ui_on_interaction();

    const struct species_def *sp = species_get_by_id((uint8_t)species_id);
    if (!sp) return;

    if (!engine_is_species_unlocked(&ctx->save, (uint8_t)species_id)) {
        ui_popup_show_reward("Locked", "Species not unlocked yet. Trigger via events.");
        hal_audio_play(SOUND_WARNING);
        return;
    }

    if (engine_buy_species(&ctx->save, (uint8_t)species_id)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Got: %s", sp->name);
        ui_popup_show_reward("Purchase Success", msg);
        hal_audio_play(SOUND_REWARD);
        // Update coin display
        if (g_shop_coin_lbl) {
            char buf[32];
            snprintf(buf, sizeof(buf), "$ %lu", (unsigned long)ctx->save.photosynth_coins);
            lv_label_set_text(g_shop_coin_lbl, buf);
        }
    } else {
        ui_popup_show_reward("Purchase Failed", "Not enough coins or creature limit reached");
        hal_audio_play(SOUND_WARNING);
    }
}

// Fill all species cards (scrollable list)
static void shop_fill_all(const struct game_save *save)
{
    if (!g_shop_grid) return;

    // Clear old content
    lv_obj_clean(g_shop_grid);

    for (int i = 0; i < species_get_count(); i++) {
        const struct species_def *sp = species_get_by_id(i + 1);
        if (!sp) continue;

        bool unlocked = save ? ((save->species_unlocked & (1ULL << i)) != 0) : false;
        uint32_t price = engine_get_species_price(i + 1);

        // Card container: full width with padding
        lv_obj_t *card = lv_obj_create(g_shop_grid);
        lv_obj_set_size(card, 336, 148);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_bg_color(card,
            unlocked ? lv_color_make(30, 45, 60) : lv_color_make(30, 30, 30), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card,
            unlocked ? lv_color_make(0, 150, 200) : lv_color_make(60, 60, 60), 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // Species name (larger, centered vertically in upper half)
        lv_obj_t *name = lv_label_create(card);
        lv_obj_set_pos(name, 16, 20);
        lv_obj_set_width(name, 220);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(name,
            unlocked ? lv_color_white() : lv_color_make(100, 100, 100), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_label_set_text(name, unlocked ? sp->name : "???");

        // Price tag (larger, below name)
        lv_obj_t *price_lbl = lv_label_create(card);
        lv_obj_set_pos(price_lbl, 16, 52);
        lv_obj_set_style_text_color(price_lbl, lv_color_make(255, 200, 0), 0);
        lv_obj_set_style_text_font(price_lbl, &lv_font_montserrat_14, 0);
        char price_buf[16];
        snprintf(price_buf, sizeof(price_buf), "$%lu", (unsigned long)price);
        lv_label_set_text(price_lbl, unlocked ? price_buf : "Locked");

        // Trophic level color dot (larger)
        lv_obj_t *troph_dot = lv_obj_create(card);
        lv_obj_set_size(troph_dot, 16, 16);
        lv_obj_set_pos(troph_dot, 296, 20);
        lv_obj_set_style_radius(troph_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(troph_dot, get_trophic_color(sp->trophic_level), 0);
        lv_obj_set_style_border_width(troph_dot, 0, 0);

        // Buy button (larger, right side)
        lv_obj_t *buy_btn = lv_btn_create(card);
        lv_obj_set_size(buy_btn, 80, 40);
        lv_obj_set_pos(buy_btn, 236, 80);
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

    // Shop scrollable list container
    g_shop_grid = lv_obj_create(g_shop_screen);
    lv_obj_set_size(g_shop_grid, 352, 390);
    lv_obj_set_pos(g_shop_grid, 8, 56);
    lv_obj_set_style_bg_opa(g_shop_grid, LV_OPA_0, 0);
    lv_obj_set_style_border_width(g_shop_grid, 0, 0);
    lv_obj_set_flex_flow(g_shop_grid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_shop_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(g_shop_grid, 8, 0);
    lv_obj_add_flag(g_shop_grid, LV_OBJ_FLAG_SCROLLABLE);
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

    // Fill all items
    shop_fill_all(save);

    lv_scr_load(g_shop_screen);
}

void ui_screen_shop_hide(void)
{
    // Navigation handled by ui_navigate_home
}