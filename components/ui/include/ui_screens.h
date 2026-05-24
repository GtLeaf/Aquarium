#pragma once

#include "lvgl.h"
#include "engine_main.h"

#ifdef __cplusplus
extern "C" {
#endif

// 主界面创建/更新
void ui_screen_main_create(void);
void ui_screen_main_update(const struct game_context *ctx);

// 设置界面
void ui_screen_settings_create(void);
void ui_screen_settings_show(void);
void ui_screen_settings_hide(void);

// 图鉴界面
void ui_screen_collection_create(void);
void ui_screen_collection_show(const struct game_save *save);
void ui_screen_collection_hide(void);

// 商店界面
void ui_screen_shop_create(void);
void ui_screen_shop_show(const struct game_save *save);
void ui_screen_shop_hide(void);
void ui_shop_stop_fill(void);  // 停止分批填充 timer

// 标题画面
void ui_screen_title_create(void);
void ui_screen_title_show(void);
bool ui_screen_title_is_visible(void);
void ui_screen_title_update(const struct game_save *save);

// 获取当前屏幕
lv_obj_t* ui_get_main_screen(void);

// 桌面伴侣模式（Ambient Mode）
void ui_ambient_enter(void);
void ui_ambient_exit(void);
bool ui_ambient_is_active(void);
void ui_ambient_update(void);

#ifdef __cplusplus
}
#endif