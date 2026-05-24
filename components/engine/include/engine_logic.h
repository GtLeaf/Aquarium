#pragma once

#include "engine_main.h"

#ifdef __cplusplus
extern "C" {
#endif

// 生态逻辑（每秒调用一次）
void engine_logic_update(struct game_context *ctx);

// 物理位置更新（每帧调用，平滑运动）
void engine_physics_update(struct game_context *ctx);

// IMU 效果（每帧调用，内部有节流）
void apply_shake_effect(struct game_context *ctx);
void apply_tilt_effect(struct game_context *ctx);

#ifdef __cplusplus
}
#endif
