#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAVE_KEY_GAMESAVE   "gamesave"
#define SAVE_KEY_BACKUP     "gamesave_bk"
#define SAVE_MAGIC          0x544F4345  // 'ECOT'
#define SAVE_VERSION        1

#define AUTO_SAVE_INTERVAL_MS   (5 * 60 * 1000)  // 5分钟
#define AUTO_SAVE_RETRY_MAX     3

// 前向声明（调用者需包含 species_data.h）
struct game_save;

// 存档管理器初始化
esp_err_t save_manager_init(void);

// 底层读写（带CRC校验）
esp_err_t save_write(const char *key, const void *data, size_t len);
esp_err_t save_read(const char *key, void *data, size_t len);

// 游戏存档操作
esp_err_t save_gamesave_write(const struct game_save *save);
esp_err_t save_gamesave_read(struct game_save *save);
bool save_gamesave_exists(void);

// 初始化默认存档
void save_gamesave_init_default(struct game_save *save);

// 自动存档（由引擎定时调用）
esp_err_t save_auto_save(const struct game_save *save);
void save_auto_save_tick(uint32_t dt_ms, const struct game_save *save);

// 存档校验
uint32_t save_crc32(const void *data, size_t len);
bool save_validate(const struct game_save *save);

// 导入/导出（返回动态分配缓冲区，调用者负责释放）
esp_err_t save_export_blob(uint8_t **out_buf, size_t *out_len, const struct game_save *save);
esp_err_t save_import_blob(const uint8_t *buf, size_t len, struct game_save *save);

// 删除存档（重置游戏）
esp_err_t save_gamesave_delete(void);

#ifdef __cplusplus
}
#endif
