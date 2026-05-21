#include "ui_popup.h"
#include "ui_screens.h"
#include "engine_main.h"
#include "event_system.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_popup";

static lv_obj_t *g_popup_overlay = NULL;
static lv_obj_t *g_popup_panel = NULL;
static lv_obj_t *g_popup_title = NULL;
static lv_obj_t *g_popup_msg = NULL;
static lv_obj_t *g_popup_btn = NULL;
static enum popup_type g_popup_type = POPUP_NONE;
static uint8_t g_popup_event_id = 0;
static bool g_popup_visible = false;

static void popup_create_base(void)
{
    if (g_popup_overlay) return;

    lv_obj_t *parent = lv_scr_act();
    if (!parent) parent = ui_get_main_screen();
    if (!parent) return;

    // 半透明遮罩
    g_popup_overlay = lv_obj_create(parent);
    lv_obj_set_size(g_popup_overlay, 368, 448);
    lv_obj_set_pos(g_popup_overlay, 0, 0);
    // lv_obj_set_style_bg_color(g_popup_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_popup_overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(g_popup_overlay, 0, 0);
    lv_obj_set_style_pad_all(g_popup_overlay, 0, 0);
    lv_obj_clear_flag(g_popup_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // 弹窗面板
    g_popup_panel = lv_obj_create(g_popup_overlay);
    lv_obj_set_size(g_popup_panel, 300, 180);
    lv_obj_center(g_popup_panel);
    // lv_obj_set_style_bg_color(g_popup_panel, lv_color_make(30, 40, 50), 0);
    lv_obj_set_style_radius(g_popup_panel, 12, 0);
    lv_obj_set_style_border_color(g_popup_panel, lv_color_make(0, 150, 200), 0);
    lv_obj_set_style_border_width(g_popup_panel, 2, 0);
    lv_obj_set_style_pad_all(g_popup_panel, 16, 0);
    lv_obj_clear_flag(g_popup_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    g_popup_title = lv_label_create(g_popup_panel);
    lv_obj_set_width(g_popup_title, 260);
    lv_obj_set_style_text_color(g_popup_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_popup_title, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(g_popup_title, LV_LABEL_LONG_WRAP);
    lv_label_set_text(g_popup_title, "Title");

    // 消息
    g_popup_msg = lv_label_create(g_popup_panel);
    lv_obj_set_pos(g_popup_msg, 16, 50);
    lv_obj_set_width(g_popup_msg, 260);
    lv_obj_set_style_text_color(g_popup_msg, lv_color_make(180, 200, 220), 0);
    lv_obj_set_style_text_font(g_popup_msg, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(g_popup_msg, LV_LABEL_LONG_WRAP);
    lv_label_set_text(g_popup_msg, "Message");

    // 按钮
    g_popup_btn = lv_btn_create(g_popup_panel);
    lv_obj_set_size(g_popup_btn, 100, 36);
    lv_obj_set_pos(g_popup_btn, 100, 120);
    // lv_obj_set_style_bg_color(g_popup_btn, lv_color_make(0, 120, 180), 0);
    lv_obj_set_style_radius(g_popup_btn, 8, 0);

    lv_obj_t *lbl = lv_label_create(g_popup_btn);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_label_set_text(lbl, "OK");
}

static void popup_show(const char *title, const char *msg, enum popup_type type)
{
    popup_create_base();
    if (!g_popup_overlay) return;

    lv_label_set_text(g_popup_title, title ? title : "");
    lv_label_set_text(g_popup_msg, msg ? msg : "");
    g_popup_type = type;
    g_popup_visible = true;

    lv_obj_clear_flag(g_popup_overlay, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Popup shown: type=%d", type);
}

void ui_popup_show_event(uint8_t event_id, const char *title, const char *desc)
{
    g_popup_event_id = event_id;
    popup_show(title, desc, POPUP_EVENT);
}

void ui_popup_show_offline(uint32_t coins, uint32_t hours, uint8_t creatures)
{
    char msg[128];
    snprintf(msg, sizeof(msg),
             "离线了 %lu 小时\n"
             "%d 只生物存活\n"
             "获得 %lu 光合币",
             (unsigned long)hours, creatures, (unsigned long)coins);
    popup_show("欢迎回来", msg, POPUP_OFFLINE);
}

void ui_popup_show_reward(const char *title, const char *msg)
{
    popup_show(title, msg, POPUP_REWARD);
}

void ui_popup_show_achievement(const char *name, const char *desc)
{
    char title[64];
    snprintf(title, sizeof(title), "Achievement Unlocked!\n%s", name ? name : "");
    popup_show(title, desc, POPUP_ACHIEVEMENT);
}

void ui_popup_close(void)
{
    if (g_popup_overlay) {
        lv_obj_add_flag(g_popup_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    g_popup_visible = false;
    g_popup_type = POPUP_NONE;
    g_popup_event_id = 0;
    ESP_LOGI(TAG, "Popup closed");
}

bool ui_popup_is_visible(void)
{
    return g_popup_visible;
}

void ui_popup_on_click(void)
{
    if (!g_popup_visible) return;

    // 应用事件奖励
    if (g_popup_type == POPUP_EVENT && g_popup_event_id > 0) {
        struct game_context *ctx = engine_get_context();
        struct event_trigger_state *estate = engine_get_event_state();
        if (ctx && estate) {
            engine_apply_event_reward(&ctx->save, g_popup_event_id);
            event_system_mark_viewed(estate, g_popup_event_id);
        }
    }

    ui_popup_close();
}
