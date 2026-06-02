#include "ui_main.h"
#include "ui_screens.h"
#include "ui_popup.h"
#include "engine_main.h"
#include "event_system.h"
#include "achievement_system.h"
#include "save_manager.h"
#include "hal_audio.h"
#include "hal_lvgl.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ui";

// 无操作计时器（毫秒）
static uint32_t s_idle_timer_ms = 0;
#define AMBIENT_ENTER_MS (60 * 1000) // 60秒无操作进入伴侣模式
#define AMBIENT_UPDATE_MS 1000       // 伴侣模式 1 FPS

// 离线收益弹窗已显示标记
static bool s_offline_popup_shown = false;

// 成就解锁回调（由成就系统触发）
static void on_achievement_unlocked(uint8_t achv_id, const char *name, const char *desc)
{
    (void)achv_id;
    ESP_LOGI(TAG, "Achievement popup: %s - %s", name, desc);
    ui_popup_show_achievement(name, desc);
    hal_audio_play(SOUND_REWARD);
}

esp_err_t ui_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "UI initializing...");

    ESP_LOGI(TAG, "Creating main screen...");
    ui_screen_main_create();
    ESP_LOGI(TAG, "Main screen created");

    ESP_LOGI(TAG, "Creating title screen...");
    ui_screen_title_create();
    ESP_LOGI(TAG, "Title screen created");

    // 注册触摸交互回调（通知 UI 用户有操作，重置 idle 计时器）
    hal_lvgl_set_interaction_cb(ui_on_interaction);

    // 注册成就解锁回调
    achievement_set_callback(on_achievement_unlocked);

    // 显示标题画面并更新存档统计
    struct game_context *ctx = engine_get_context();
    if (ctx) {
        ESP_LOGI(TAG, "Updating title with save data...");
        ui_screen_title_update(&ctx->save);
    } else {
        ESP_LOGW(TAG, "No game context, showing title without save data");
    }

    ESP_LOGI(TAG, "Showing title screen...");
    ui_screen_title_show();
    ESP_LOGI(TAG, "Title screen shown");

    ESP_LOGI(TAG, "UI initialized successfully");
    ESP_LOGI(TAG, "========================================");
    return ESP_OK;
}

void ui_update(void)
{
    struct game_context *ctx = engine_get_context();
    if (!ctx) return;

    // 状态机：TANK_VIEW <-> AMBIENT_MODE
    if (ctx->state == STATE_TANK_VIEW) {
        s_idle_timer_ms += ENGINE_TICK_MS;

        ESP_LOGD(TAG, "s_idle_timer_ms：%d", s_idle_timer_ms);
        // 检查是否需要进入伴侣模式
        if (s_idle_timer_ms >= AMBIENT_ENTER_MS) {
            engine_set_state(STATE_AMBIENT_MODE);
            ui_ambient_enter();
            s_idle_timer_ms = 0;
            return;
        }

        // 如果有弹窗显示中，不更新主界面
        if (ui_popup_is_visible()) {
            return;
        }

        ui_screen_main_update(ctx);

        // 检查是否有待显示事件（每 2 秒检查一次）
        static uint32_t event_check_timer = 0;
        event_check_timer += ENGINE_TICK_MS;
        if (event_check_timer >= 2000) {
            event_check_timer = 0;
            struct event_trigger_state *estate = engine_get_event_state();
            if (estate && event_system_has_pending(estate)) {
                const struct active_event *ae = event_system_get_next_pending(estate);
                if (ae) {
                    const struct event_def *ev = event_get_by_id(ae->event_id);
                    if (ev) {
                        ui_popup_show_event(ae->event_id, ev->name, ev->desc);
                    }
                }
            }
        }
    } else if (ctx->state == STATE_AMBIENT_MODE) {
        ui_ambient_update();
    }

    // FPS 监控（每 60 帧输出一次）
    #if CONFIG_LV_USE_PERF_MONITOR
    static uint32_t fps_frame = 0;
    static uint32_t fps_time = 0;
    fps_frame++;
    fps_time += 16;
    if (fps_frame >= 60) {
        ESP_LOGD(TAG, "FPS: %.1f", 1000.0f * fps_frame / fps_time);
        fps_frame = 0;
        fps_time = 0;
    }
    #endif
}

// 用户交互时重置空闲计时器并退出伴侣模式
void ui_on_interaction(void)
{
    ESP_LOGD(TAG, "ui_on_interaction");
    s_idle_timer_ms = 0;
    if (ui_ambient_is_active()) {
        ui_ambient_exit();
        engine_set_state(STATE_TANK_VIEW);
    }
}

// 显示离线收益弹窗（由 engine_init 调用）
void ui_show_offline_popup(uint32_t coins, uint32_t hours, uint8_t creatures)
{
    if (coins > 0 && !s_offline_popup_shown) {
        ui_popup_show_offline(coins, hours, creatures);
        s_offline_popup_shown = true;
    }
}

// 导航到设置界面
void ui_navigate_settings(void)
{
    engine_set_state(STATE_SETTINGS);
    ui_screen_settings_show();
}

// 导航到图鉴界面
void ui_navigate_collection(void)
{
    struct game_context *ctx = engine_get_context();
    if (ctx) {
        ui_screen_collection_show(&ctx->save);
    }
}

// 返回主界面
void ui_navigate_home(void)
{
    // 停止商店/图鉴分批填充 timer（如果正在进行）
    ui_shop_stop_fill();
    ui_collection_stop_fill();

    // 关闭可能残留的弹窗（否则 g_popup_visible 阻塞 ui_screen_main_update）
    if (ui_popup_is_visible()) {
        ui_popup_close();
    }

    ui_screen_shop_hide();
    ui_screen_settings_hide();
    ui_screen_collection_hide();

    // 恢复生态运转
    engine_set_state(STATE_TANK_VIEW);

    // 清理子页面的重对象，释放内存并减少下次渲染负担
    extern lv_obj_t *g_shop_grid;
    extern lv_obj_t *g_collection_grid;
    if (g_shop_grid) lv_obj_clean(g_shop_grid);
    if (g_collection_grid) lv_obj_clean(g_collection_grid);

    // 从设置页面返回时，立即存档当前设置
    struct game_context *ctx = engine_get_context();
    if (ctx && ctx->dirty) {
        save_gamesave_write(&ctx->save);
        ESP_LOGI("ui", "Settings saved on exit");
        ctx->dirty = false;
    }

    lv_obj_t *main = ui_get_main_screen();
    if (main) {
        lv_scr_load(main);
    }

    // 立即刷新一帧主界面（让新购入的生物马上显示）
    if (ctx) {
        ui_screen_main_update(ctx);
    }
}
