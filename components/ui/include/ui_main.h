#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ui_init(void);
void ui_update(void);
void ui_on_interaction(void);
void ui_show_offline_popup(uint32_t coins, uint32_t hours, uint8_t creatures);

// 界面导航
void ui_navigate_settings(void);
void ui_navigate_collection(void);
void ui_navigate_home(void);

#ifdef __cplusplus
}
#endif
