#include "save_manager.h"
#include "species_data.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "save_manager";
static nvs_handle_t g_nvs_handle = 0;
static uint32_t s_auto_save_timer = 0;
static uint8_t s_retry_count = 0;

// ========== 异步存档任务 ==========
#define SAVE_TASK_STACK_SIZE  4096
#define SAVE_TASK_PRIORITY    2        // 低于 lvgl_task(5) 和 main(5)

static TaskHandle_t      s_save_task_handle = NULL;
static SemaphoreHandle_t s_save_mutex       = NULL;
static struct game_save  s_save_buffer;     // 双缓冲：主线程写 → 存档线程读

// CRC32 查找表（IEEE 802.3 多项式）
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t save_crc32(const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

esp_err_t save_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open("storage", NVS_READWRITE, &g_nvs_handle);
    ESP_LOGI(TAG, "NVS save manager init");
    return ret;
}

esp_err_t save_write(const char *key, const void *data, size_t len)
{
    if (!g_nvs_handle || !key || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = nvs_set_blob(g_nvs_handle, key, data, len);
    if (ret == ESP_OK) {
        ret = nvs_commit(g_nvs_handle);
    }
    return ret;
}

esp_err_t save_read(const char *key, void *data, size_t len)
{
    if (!g_nvs_handle || !key || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t required = len;
    return nvs_get_blob(g_nvs_handle, key, data, &required);
}

bool save_gamesave_exists(void)
{
    if (!g_nvs_handle) return false;
    size_t len = 0;
    esp_err_t ret = nvs_get_blob(g_nvs_handle, SAVE_KEY_GAMESAVE, NULL, &len);
    return (ret == ESP_OK && len == sizeof(struct game_save));
}

void save_gamesave_init_default(struct game_save *save)
{
    if (!save) return;
    memset(save, 0, sizeof(struct game_save));

    save->magic = SAVE_MAGIC;
    save->version = SAVE_VERSION;

    // 默认环境
    save->env.sunlight = 60;
    save->env.nutrients = 40;
    save->env.oxygen = 80;
    save->env.algae_mass = 30;
    save->env.temperature = 25;
    save->env.is_daytime = true;
    save->env.total_seconds = 0;

    // 默认生物：螺旋藻 + 黑壳虾 + 灯科鱼
    save->creature_count = 3;
    save->creatures[0].creature_id = 1;
    save->creatures[0].species_id = 1;  // Spirulina 螺旋藻
    save->creatures[0].stage = STAGE_ADULT;
    save->creatures[0].size = 15;
    save->creatures[0].pos_x = 100;
    save->creatures[0].pos_y = 200;
    save->creatures[0].hunger = 0;
    save->creatures[0].mood = 80;

    save->creatures[1].creature_id = 2;
    save->creatures[1].species_id = 6;  // 黑壳虾
    save->creatures[1].stage = STAGE_ADULT;
    save->creatures[1].size = 20;
    save->creatures[1].pos_x = 150;
    save->creatures[1].pos_y = 250;
    save->creatures[1].hunger = 20;
    save->creatures[1].mood = 70;

    save->creatures[2].creature_id = 3;
    save->creatures[2].species_id = 10; // 灯科鱼
    save->creatures[2].stage = STAGE_ADULT;
    save->creatures[2].size = 25;
    save->creatures[2].pos_x = 200;
    save->creatures[2].pos_y = 220;
    save->creatures[2].hunger = 30;
    save->creatures[2].mood = 75;

    // 默认解锁 L1 基础物种
    save->species_unlocked = (1ULL << 0) | (1ULL << 1) | (1ULL << 2) |
                              (1ULL << 5) | (1ULL << 9);

    save->photosynth_coins = 10000;
    save->tank_level = 1;
    save->play_days_total = 0;
    save->today_events = 0;
    save->day_of_year = 0;

    save->brightness = 255;
    save->time_speed = 1;  // 默认 1x
    save->ambient_timeout = 1;  // 默认 60s

    // 计算 CRC
    save->crc32 = save_crc32(save, offsetof(struct game_save, crc32));
}

esp_err_t save_gamesave_write(const struct game_save *save)
{
    if (!save) return ESP_ERR_INVALID_ARG;
    if (save->magic != SAVE_MAGIC) {
        ESP_LOGE(TAG, "Invalid save magic");
        return ESP_ERR_INVALID_STATE;
    }

    // 更新时间戳和离线开始时间
    struct game_save tmp = *save;
    time_t now = time(NULL);
    tmp.last_save_time = (uint32_t)now;
    if (tmp.offline_start == 0) {
        tmp.offline_start = (uint32_t)now;
    }
    tmp.crc32 = 0;
    tmp.crc32 = save_crc32(&tmp, offsetof(struct game_save, crc32));

    // 双写保护：先写备份，再写主存档
    esp_err_t ret = save_write(SAVE_KEY_BACKUP, &tmp, sizeof(tmp));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Backup write failed: %s", esp_err_to_name(ret));
    }

    ret = save_write(SAVE_KEY_GAMESAVE, &tmp, sizeof(tmp));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Game saved, creatures=%d, coins=%lu",
                 tmp.creature_count, (unsigned long)tmp.photosynth_coins);
        s_retry_count = 0;
    } else {
        ESP_LOGE(TAG, "Save failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

// 版本迁移：从旧版本存档迁移到新版本
static void save_migrate(struct game_save *save, uint32_t old_version)
{
    if (!save) return;

    ESP_LOGI(TAG, "Migrating save from version %lu to %d", (unsigned long)old_version, SAVE_VERSION);

    if (old_version < 3) {
        // v1/v2 -> v3: 新增 achievements_unlocked 字段
        save->achievements_unlocked = 0;
    }
    if (old_version < 4) {
        // v3 -> v4: 新增 brightness 和 time_speed 字段
        save->brightness = 255;
        save->time_speed = 1;
    }

    save->version = SAVE_VERSION;
    ESP_LOGI(TAG, "Save migrated successfully");
}

esp_err_t save_gamesave_read(struct game_save *save)
{
    if (!save) return ESP_ERR_INVALID_ARG;

    // 先尝试读取主存档
    esp_err_t ret = save_read(SAVE_KEY_GAMESAVE, save, sizeof(struct game_save));
    if (ret == ESP_OK) {
        if (save_validate(save)) {
            ESP_LOGI(TAG, "Main save loaded, creatures=%d", save->creature_count);
            return ESP_OK;
        }
        // 版本不匹配，尝试迁移
        if (save->magic == SAVE_MAGIC && save->version < SAVE_VERSION) {
            ESP_LOGW(TAG, "Save version old: %lu, migrating...", (unsigned long)save->version);
            save_migrate(save, save->version);
            // 迁移后重新计算 CRC 并保存
            save->crc32 = 0;
            save->crc32 = save_crc32(save, offsetof(struct game_save, crc32));
            save_write(SAVE_KEY_GAMESAVE, save, sizeof(struct game_save));
            return ESP_OK;
        }
    }

    // 主存档损坏或无法迁移，尝试备份
    ESP_LOGW(TAG, "Main save invalid or missing, trying backup...");
    ret = save_read(SAVE_KEY_BACKUP, save, sizeof(struct game_save));
    if (ret == ESP_OK) {
        if (save_validate(save)) {
            ESP_LOGI(TAG, "Backup save loaded");
            // 恢复主存档
            save_write(SAVE_KEY_GAMESAVE, save, sizeof(struct game_save));
            return ESP_OK;
        }
        if (save->magic == SAVE_MAGIC && save->version < SAVE_VERSION) {
            ESP_LOGW(TAG, "Backup save version old: %lu, migrating...", (unsigned long)save->version);
            save_migrate(save, save->version);
            save->crc32 = 0;
            save->crc32 = save_crc32(save, offsetof(struct game_save, crc32));
            save_write(SAVE_KEY_GAMESAVE, save, sizeof(struct game_save));
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "No valid save found, initializing default");
    save_gamesave_init_default(save);
    return ESP_OK; // 返回 OK，但数据是默认的
}

bool save_validate(const struct game_save *save)
{
    if (!save) return false;
    if (save->magic != SAVE_MAGIC) {
        ESP_LOGW(TAG, "Bad magic: 0x%08lx", (unsigned long)save->magic);
        return false;
    }
    // 允许旧版本通过验证，由调用者执行迁移
    if (save->version > SAVE_VERSION) {
        ESP_LOGW(TAG, "Version too new: %lu vs %d", (unsigned long)save->version, SAVE_VERSION);
        return false;
    }
    uint32_t crc = save_crc32(save, offsetof(struct game_save, crc32));
    if (crc != save->crc32) {
        ESP_LOGW(TAG, "CRC mismatch: calc=0x%08lx, stored=0x%08lx",
                 (unsigned long)crc, (unsigned long)save->crc32);
        return false;
    }
    return true;
}

esp_err_t save_auto_save(const struct game_save *save)
{
    esp_err_t ret = save_gamesave_write(save);
    if (ret != ESP_OK && s_retry_count < AUTO_SAVE_RETRY_MAX) {
        s_retry_count++;
        ESP_LOGW(TAG, "Auto-save retry %d/%d", s_retry_count, AUTO_SAVE_RETRY_MAX);
    }
    return ret;
}

void save_auto_save_tick(uint32_t dt_ms, const struct game_save *save)
{
    s_auto_save_timer += dt_ms;
    if (s_auto_save_timer >= AUTO_SAVE_INTERVAL_MS) {
        s_auto_save_timer = 0;

        // 如果异步任务未启动，回退到同步写入
        if (!s_save_task_handle) {
            save_auto_save(save);
            return;
        }

        // 将当前存档数据拷贝到缓冲区，然后通知存档任务
        if (xSemaphoreTake(s_save_mutex, 0) == pdTRUE) {
            memcpy(&s_save_buffer, save, sizeof(s_save_buffer));
            xSemaphoreGive(s_save_mutex);
            xTaskNotifyGive(s_save_task_handle);
        } else {
            ESP_LOGW(TAG, "save buffer busy, skip this save cycle");
        }
    }
}

esp_err_t save_export_blob(uint8_t **out_buf, size_t *out_len, const struct game_save *save)
{
    if (!out_buf || !out_len || !save) return ESP_ERR_INVALID_ARG;
    size_t len = sizeof(struct game_save);
    uint8_t *buf = (uint8_t *)malloc(len);
    if (!buf) return ESP_ERR_NO_MEM;
    memcpy(buf, save, len);
    *out_buf = buf;
    *out_len = len;
    return ESP_OK;
}

esp_err_t save_import_blob(const uint8_t *buf, size_t len, struct game_save *save)
{
    if (!buf || !save || len != sizeof(struct game_save)) return ESP_ERR_INVALID_ARG;
    struct game_save tmp;
    memcpy(&tmp, buf, sizeof(tmp));
    if (!save_validate(&tmp)) {
        ESP_LOGE(TAG, "Import blob validation failed");
        return ESP_ERR_INVALID_CRC;
    }
    *save = tmp;
    return ESP_OK;
}

esp_err_t save_gamesave_delete(void)
{
    if (!g_nvs_handle) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = nvs_erase_key(g_nvs_handle, SAVE_KEY_GAMESAVE);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase main save: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = nvs_erase_key(g_nvs_handle, SAVE_KEY_BACKUP);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to erase backup save: %s", esp_err_to_name(ret));
    }
    ret = nvs_commit(g_nvs_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Game save deleted");
    }
    return ret;
}

// ========== 异步存档任务实现 ==========

static void save_task(void *arg)
{
    (void)arg;
    for (;;) {
        // 阻塞等待通知（来自 save_auto_save_tick）
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 在互斥锁下拷贝数据到本地栈变量
        struct game_save local_copy;
        if (xSemaphoreTake(s_save_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            memcpy(&local_copy, &s_save_buffer, sizeof(local_copy));
            xSemaphoreGive(s_save_mutex);
        } else {
            ESP_LOGW(TAG, "mutex timeout, skip this save cycle");
            continue;
        }

        // 执行耗时的 NVS 写入（此时不阻塞主线程）
        save_gamesave_write(&local_copy);
        ESP_LOGI(TAG, "async save completed");
    }
}

void save_manager_start_task(void)
{
    if (s_save_task_handle) return;  // 防止重复创建

    s_save_mutex = xSemaphoreCreateMutex();
    assert(s_save_mutex);

    BaseType_t ret = xTaskCreatePinnedToCore(
        save_task,
        "save_task",
        SAVE_TASK_STACK_SIZE,
        NULL,
        SAVE_TASK_PRIORITY,
        &s_save_task_handle,
        1              // 绑定到 CPU1（与 lvgl_task 分开）
    );
    assert(ret == pdPASS);
    ESP_LOGI(TAG, "async save task started on CPU1");
}
