# AGENTS.md — 像素生态缸（ESP32-S3 固件项目）

> 本文档供 AI 编码智能体阅读。阅读者应对本项目一无所知，所有信息必须基于实际代码和配置文件，不做假设。

---

## 1. 项目概述

**像素生态缸 · 治愈放置版**（Tiny Eco Tank）是一款运行在微雪 ESP32-S3-Touch-AMOLED-1.8 开发板上的像素风桌面生态缸放置游戏。

- **芯片目标**：ESP32-S3（双核 Xtensa LX7，主频 240MHz）
- **屏幕**：1.8 英寸 AMOLED，分辨率 368×448，驱动芯片 SH8601，QSPI 接口
- **交互**：FT3168 电容触摸（I2C）、QMI8658 六轴 IMU（I2C）、PWM 蜂鸣器（GPIO40）
- **电源管理**：AXP2101 PMU（I2C）+ 锂电池
- **RTC**：PCF85063（I2C）
- **项目根目录**：`d:\Study\ESP32\hello_world`
- **固件名称**：`esp32s3_touch_amoled_18.bin`

核心玩法：低操作、长陪伴的放置养成。玩家通过随机事件（海鸥/漂流瓶/海风/天气等）获得生物，观察食物链生态循环，收集图鉴，离线产出光合币。

---

## 2. 技术栈与构建系统

| 层级 | 技术 |
|------|------|
| 操作系统 / RTOS | FreeRTOS（ESP-IDF 内置） |
| 框架 | ESP-IDF v6.0.1 |
| 构建系统 | CMake + Ninja |
| 语言标准 | C11，C++17（几乎无 C++ 代码） |
| UI 框架 | LVGL 9.5.0（ managed_component `lvgl/lvgl`） |
| 显示驱动 | Waveshare SH8601 QSPI（managed_component `waveshare/esp_lcd_sh8601`） |
| 数据持久化 | NVS（Non-Volatile Storage） |
| 文件系统 | LittleFS（预留分区） |
| 包管理 | ESP-IDF Component Manager（`idf_component.yml`） |

### 2.1 关键外部依赖

- `lvgl/lvgl: *` — UI 渲染框架
- `waveshare/esp_lcd_sh8601: *` — AMOLED 面板驱动
- `espressif/cmake_utilities: 0.*` — SH8601 驱动的辅助构建工具

---

## 3. 项目结构与代码组织

项目采用 ESP-IDF 标准 **CMake 组件化**结构：

```
hello_world/
├── CMakeLists.txt              # 项目级 CMake，设定 IDF_TARGET=esp32s3
├── sdkconfig                   # 完整 Kconfig 生成配置（自动生成，勿手改）
├── sdkconfig.defaults          # 自定义默认值（PSRAM、LVGL、栈大小等）
├── partitions.csv              # 自定义分区表（factory 2MB + littlefs ~2MB）
├── main/
│   ├── CMakeLists.txt          # main 组件，依赖 eco_hal/ui/engine/save/utils
│   ├── idf_component.yml       # 组件依赖声明（lvgl、sh8601）
│   └── main.c                  # 程序入口 app_main()，按序初始化所有模块
├── components/                 # 自定义组件（按业务域拆分）
│   ├── eco_hal/                # 硬件抽象层（HAL）
│   │   ├── hal_i2c.c           # I2C 总线初始化（SDA=GPIO15, SCL=GPIO14）
│   │   ├── hal_display.c       # SH8601 QSPI AMOLED 显示初始化
│   │   ├── hal_touch.c         # FT3168 触摸驱动（含 PCA9554 复位序列）
│   │   ├── hal_lvgl.c          # LVGL 显示端口、颜色格式 RGB565_SWAPPED
│   │   ├── hal_pmu.c           # AXP2101 电源管理（ALDO1/2/4 供电配置）
│   │   ├── hal_rtc.c           # PCF85063 RTC
│   │   ├── hal_imu.c           # QMI8658 六轴 IMU
│   │   ├── hal_audio.c         # PWM 蜂鸣器驱动（GPIO40，LEDC）
│   │   └── include/*.h
│   ├── engine/                 # 游戏引擎与核心逻辑
│   │   ├── engine_main.c       # 引擎初始化、状态机、离线收益、存档 dirty 标记
│   │   ├── engine_logic.c      # 生态心跳（1Hz）：环境更新、成长、衰减
│   │   ├── event_system.c      # 24 种随机事件的触发、冷却、每日上限
│   │   ├── achievement_system.c# 成就系统
│   │   └── include/*.h
│   ├── save/                   # 存档管理
│   │   ├── save_manager.c      # NVS 读写、CRC32 校验、双写保护、自动存档
│   │   └── include/*.h
│   ├── ui/                     # LVGL 用户界面
│   │   ├── ui_main.c           # UI 初始化、状态机（TANK_VIEW / AMBIENT_MODE）
│   │   ├── ui_screens.c        # 各屏幕创建与更新（主缸景、标题、图鉴、商店、设置）
│   │   ├── ui_popup.c          # 弹窗（离线收益、事件、成就）
│   │   └── include/*.h
│   └── utils/                  # 通用工具与数据
│       ├── species_data.c      # 30 种物种定义 + 24 种事件定义（静态数据库）
│       ├── utils_helpers.c     # 毫秒 tick、延时辅助
│       └── include/*.h
├── managed_components/         # Component Manager 自动下载的依赖
│   ├── espressif__cmake_utilities/
│   ├── lvgl__lvgl/
│   └── waveshare__esp_lcd_sh8601/
├── doc/                        # 项目文档（中文）
│   ├── BUILD_GUIDE.md          # Windows PowerShell 构建与烧录命令
│   ├── debug_notes.md          # 硬件 bring-up 排障记录（I2C/触摸/颜色/栈溢出/WDT）
│   ├── 像素生态缸_PRD_v0.4_2026-05-20.md  # 产品需求文档（30 物种、事件系统、数值）
│   ├── species.csv             # 物种配置基线
│   └── events.csv              # 事件配置基线
└── build/                      # CMake 构建输出目录
```

### 3.1 组件依赖关系

```
main
├── eco_hal  ←  requires driver, esp_lcd, esp_timer, lvgl, esp_driver_ledc, esp_lcd_sh8601
├── ui       ←  requires lvgl, eco_hal, utils; priv_requires engine
├── engine   ←  requires freertos, utils, save, eco_hal
├── save     ←  requires nvs_flash, esp_partition, utils
└── utils    ←  requires freertos
```

**循环依赖禁忌**：`ui` 对 `engine` 使用 `PRIV_REQUIRES`，意味着 `ui` 的公开头文件不能包含 `engine` 的头文件；`engine` 也不应直接依赖 `ui`。

---

## 4. 运行时架构

### 4.1 启动顺序（`app_main()`）

1. `save_manager_init()` — NVS 初始化
2. `hal_i2c_init()` — I2C 总线（PMU、触摸、RTC、IMU 共用）
3. `hal_pmu_init()` — AXP2101 供电（ALDO1/2/4），失败仅警告，继续启动
4. `hal_display_init()` — QSPI + SH8601 面板点亮
5. `hal_touch_init()` — FT3168 触摸初始化（含 PCA9554 P2 复位），失败仅警告
6. `hal_audio_init()` — PWM 蜂鸣器初始化（LEDC，GPIO40）
7. `hal_lvgl_init()` — LVGL 显示端口配置
8. `engine_init()` — 读取存档、计算离线收益、初始化事件与成就系统
9. `ui_init()` — 创建 LVGL 屏幕对象树（此时无渲染任务，线程安全）
10. `xTaskCreatePinnedToCore(hal_lvgl_port_task, ..., 0)` — 在 Core 0 启动 LVGL 渲染任务
11. `engine_tick()` 主循环 — 每 16ms（~60 FPS）在 `main` 任务中运行

### 4.2 任务与核心分配

| 任务 | 核心 | 优先级 | 说明 |
|------|------|--------|------|
| `main` | 默认（通常 Core 1） | — | `app_main()` 所在任务，运行 `engine_tick()` |
| `lvgl_task` | Core 0 | 5 | LVGL 渲染与刷新（`hal_lvgl_port_task`） |

**注意**：UI 初始化（`ui_init()`）在 LVGL 任务启动前完成，避免对象树创建与渲染的竞态。

**⚠️ 线程安全（已知问题）**：`main` 任务和 `lvgl_task` 都会调用 LVGL API，但当前代码**未启用 FreeRTOS OSAL**，也未使用 `lv_lock()` / `lv_unlock()` 保护。这可能导致两个任务并发访问 LVGL 内部数据结构时触发 WDT。临时规避方案：商店/图鉴使用分批异步加载（`SHOP_BATCH_SIZE=3`），减少单次渲染对象数。

### 4.3 主循环时序（`engine_tick()`）

- **每 16ms**：调用一次，累计帧计数
- **每 1000ms（1 秒）**：
  - `engine_logic_update()` — 生态心跳（环境、成长、衰减）
  - `event_system_tick()` — 随机事件检查
  - `engine_try_breed()` — 繁殖判定（每 5 分钟一次实际尝试）
  - `achievement_check()` — 成就检查（每 10 秒一次）
- **每 60 秒**：`save_auto_save_tick()` 触发自动存档（若数据 dirty）

### 4.4 关键数据结构

全局游戏上下文：`struct game_context`（`engine_main.h`）
- `enum game_state state` — 当前状态（BOOT / TITLE / TANK_VIEW / EVENT_POPUP / AMBIENT_MODE）
- `struct game_save save` — 完整存档数据
- `bool dirty` — 数据变更标记，触发自动存档

存档结构：`struct game_save`（`species_data.h`）
- 魔数 `SAVE_MAGIC = 0x544F4345`（'ECOT'）
- 版本号 `SAVE_VERSION = 1`
- CRC32 校验
- 环境参数（阳光、营养、氧气、藻类、温度）
- 生物数组 `creatures[24]`
- 图鉴位图 `species_unlocked`（uint64，64 位）
- 光合币、缸等级、离线时间戳等

---

## 5. 构建与烧录命令

### 5.1 环境要求

- **操作系统**：Windows（PowerShell 5）
- **ESP-IDF 路径**：`D:\softwareInstall\ESPIDF\v6.0.1\esp-idf`
- **Python 虚拟环境**：`C:\Espressif\tools\python\v6.0.1\venv`
- **CMake**：`C:\Espressif\tools\cmake\4.0.3\bin`
- **Ninja**：`C:\Espressif\tools\ninja\1.12.1`

### 5.2 环境变量（每次构建前必须设置）

```powershell
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;$env:PATH"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
```

### 5.3 构建

```powershell
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;$env:PATH"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py build
```

> 若 `build/` 目录不存在，需先执行 `fullclean` + `set-target esp32s3` + `build`。

### 5.4 烧录

```powershell
# 同上设置环境变量后
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py -p COM3 -b 460800 flash
```

**COM3 被占用时**：先杀掉残留 python 进程：
```powershell
powershell -Command "Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force"
```

### 5.5 串口监视

```powershell
# 同上设置环境变量后
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py -p COM3 monitor
```
- 波特率 115200（自动）
- 按 **Ctrl+]** 退出

### 5.6 一键脚本

项目提供 PowerShell 脚本 `build_flash_monitor.ps1`：
```powershell
.\build_flash_monitor.ps1 all    # 构建 + 烧录 + 监视
.\build_flash_monitor.ps1 build  # 仅构建
.\build_flash_monitor.ps1 flash  # 仅烧录
.\build_flash_monitor.ps1 clean  # 清理
.\build_flash_monitor.ps1 fullclean  # 完全清理
```
---

## 6. 代码风格指南

### 6.1 语言与注释

- **源代码注释以中文为主**，关键术语保留英文（如 `task`、`heap`、`stack`）。
- 头文件使用 `#pragma once` 作为 include guard。
- C++ 兼容性：所有公开头文件包裹 `extern "C" { ... }`。

### 6.2 命名规范

| 类型 | 前缀/风格 | 示例 |
|------|----------|------|
| 全局函数（HAL） | `hal_` | `hal_display_init()` |
| 全局函数（引擎） | `engine_` | `engine_init()`, `engine_tick()` |
| 全局函数（存档） | `save_` | `save_manager_init()`, `save_gamesave_write()` |
| 全局函数（UI） | `ui_` | `ui_init()`, `ui_screen_main_create()` |
| 全局函数（工具） | `utils_` / `species_` | `utils_get_tick_ms()`, `species_get_by_id()` |
| 静态局部变量 | `s_` | `s_ctx`, `s_tick_ms` |
| 模块级句柄 | `g_` | `g_nvs_handle` |
| 宏 / 常量 | 全大写下划线 | `SAVE_MAGIC`, `MAX_CREATURES`, `AUTO_SAVE_INTERVAL_MS` |
| 结构体 | 小写下划线 | `struct game_save`, `struct creature` |
| ESP_LOG 标签 | 小写模块名 | `static const char *TAG = "engine";` |

### 6.3 错误处理

- 使用 `esp_err_t` 作为错误码类型。
- 初始化函数失败时：致命错误用 `ESP_ERROR_CHECK()`；非致命错误返回错误码并由调用者决定是否继续（如 PMU、触摸初始化失败仅警告）。
- 所有 `esp_err_t` 返回值应被检查或显式忽略（用 `(void)` 转换）。

### 6.4 内存管理

- 动态分配优先使用 `heap_caps_malloc(..., MALLOC_CAP_DMA)` 用于 DMA 缓冲区。
- PSRAM 可用（OCT 80MHz），通过 `CONFIG_SPIRAM_USE_MALLOC=y` 使 `malloc` 自动使用 PSRAM。
- 主任务栈大小已调整为 8192 字节（`sdkconfig.defaults`），避免 UI 初始化栈溢出。

### 6.5 硬件相关约定

- **I2C 引脚**：SDA = GPIO15，SCL = GPIO14。曾有反接导致黑屏的严重 bug，已记录在 `doc/debug_notes.md`。
- **QSPI 显示引脚**：CS=12, CLK=11, D0=4, D1=5, D2=6, D3=7, RST=17。
- **AXP2101 LDO**：ALDO1（RTC/IMU）、ALDO2（FT3168 触摸）、ALDO4（显示屏/TF 卡）。
- **颜色格式**：AMOLED 必须使用 `LV_COLOR_FORMAT_RGB565_SWAPPED`，否则颜色通道错位（见 `doc/debug_notes.md` §2）。
- **触摸复位**：FT3168 的 RST 不直接连 GPIO，而是通过 PCA9554（I2C 0x20）的 P2 引脚控制。
- **音频**：PWM 蜂鸣器接 GPIO40，使用 LEDC 驱动，非 I2S/ES8311。

---

## 7. 测试策略

### 7.1 现有测试

- `pytest_hello_world.py`：ESP-IDF 示例遗留的 pytest 脚本，当前仅检查串口输出是否包含 `"Hello world!"`，**已不符合实际固件行为**（固件启动日志为 `"ESP32-S3 Touch AMOLED 1.8 starting..."`）。
- **无单元测试框架**：当前项目无 Unity、CppUTest 或其他嵌入式测试框架集成。

### 7.2 推荐测试方式

- **硬件在环测试**：烧录到开发板，通过 `idf.py monitor` 观察启动日志和运行时日志。
- **日志验收**：每个模块初始化后输出 `========================================` 分隔的 OK 日志。
- **存档测试**：修改数据后观察 `"Game saved"` 日志；断电重启验证数据恢复。
- **触摸测试**：标题画面的 START 按钮是否响应，进入主缸景后点击是否触发喂食。
- **显示测试**：启动后应显示红/绿/蓝竖条纹测试画面（`hal_display.c` 调试用）。

---

## 8. 安全与可靠性考虑

### 8.1 存档安全

- **双写保护**：`save_gamesave_write()` 先写备份键 `gamesave_bk`，再写主键 `gamesave`。
- **读取回退**：读取时先读主存档，CRC/魔数校验失败则尝试备份；备份有效时自动恢复主存档。
- **CRC32**：IEEE 802.3 多项式，覆盖除 `crc32` 字段外的整个 `struct game_save`。
- **自动存档**：每 60 秒检查一次，失败时最多重试 3 次。

### 8.2 运行时安全

- **任务看门狗**：`CONFIG_ESP_TASK_WDT_TIMEOUT_S=10`，防止 LVGL 大面积渲染时触发复位。
- **栈溢出**：主任务栈已增加到 8192 字节。
- **NULL 检查**：引擎和存档函数普遍检查指针参数；HAL 层对 `io_handle` / `panel_handle` 做空指针保护。
- **线程安全（已知问题）**：`main` 任务与 `lvgl_task` 并发访问 LVGL API **未加锁保护**，存在竞态条件风险。触发 WDT 时检查堆栈：若卡在 `lv_inv_area` 且运行任务为 `main`，即为该问题。

### 8.3 数据校验

- `save_validate()` 检查魔数、版本号、CRC32 三重校验。
- 存档导入（`save_import_blob`）必须通过 `save_validate()`。
- 物种 ID 和事件 ID 在数据库中按线性搜索匹配，越界返回 `NULL`。

---

## 9. 开发工作流与常见问题

### 9.1 新增物种或事件

1. 修改 `components/utils/species_data.c` 中的 `species_db[]` 或 `event_db[]`。
2. 同步更新 `doc/species.csv` 或 `doc/events.csv`（PRD 配置基线）。
3. 若新增字段，更新 `components/utils/include/species_data.h` 中的结构体定义。
4. 若影响存档布局，递增 `SAVE_VERSION` 并添加版本迁移逻辑。
5. 重新构建并验证图鉴 UI 是否正常显示。

### 9.2 新增 UI 屏幕

1. 在 `components/ui/ui_screens.c` 中实现屏幕创建/显示/隐藏/更新函数。
2. 在 `components/ui/include/ui_screens.h` 中声明接口。
3. 在 `ui_main.c` 的状态机或导航函数中调用。
4. 注意 **单屏信息密度硬约束**（PRD §6.5）：可点击元素 ≤ 5 个，触控目标 ≥ 40×40 px，动效 ≤ 200ms。

### 9.3 硬件排障速查

| 现象 | 排查点 | 参考文档 |
|------|--------|----------|
| 屏幕黑屏 | I2C 引脚是否反接；AXP2101 ALDO4 是否开启 | `doc/debug_notes.md` §1 |
| 颜色异常（青/黄） | LVGL 颜色格式是否为 `RGB565_SWAPPED` | `doc/debug_notes.md` §2 |
| 触摸无响应 | PCA9554 P2 复位序列；FT3168 工作模式 0x00 | `doc/debug_notes.md` §3 |
| 栈溢出重启 | 主任务栈是否 ≥ 8192 | `doc/debug_notes.md` §4 |
| WDT 触发（main 任务） | `main` 任务是否未加锁调用 LVGL API | `doc/debug_notes.md` §5 |
| WDT 触发（lvgl_task） | 单次渲染对象是否过多；是否使用分批加载 | `doc/debug_notes.md` §5 |
| 构建失败（找不到 cmake） | 环境变量 PATH 是否包含 cmake 和 ninja | `doc/BUILD_GUIDE.md` §6 |

---

## 10. 已知问题与限制

### 10.1 食物链未实现

当前 `engine_logic.c` 中 **没有实现捕食逻辑**：
- L1 生产者：不饥饿，不会死亡（除非摇晃事件）
- L2/L3/L4 消费者：会饥饿，但 **不会主动捕食**，只能通过摇晃事件（`SHAKE_EFFECT_FEED`）降低饥饿值
- 若需实现食物链，需在 `update_creatures()` 中添加：L2 消耗藻类、L3/L4 捕食低营养级生物

### 10.2 商店分批加载

商店使用 **分批异步加载**（每 30ms 创建 3 个卡片），而非虚拟列表。进入商店时会看到卡片逐批出现。返回主界面时调用 `ui_shop_stop_fill()` 停止未完成的加载。

### 10.3 图鉴同样使用分批加载

图鉴屏幕与商店类似，使用 `COLLECTION_BATCH_SIZE` 分批创建卡片，避免一次性创建 30 个对象触发 WDT。

---

## 11. 文件修改禁忌

- **不要手动修改 `sdkconfig`**：应通过 `idf.py menuconfig` 修改，或在 `sdkconfig.defaults` 中添加默认值。
- **不要修改 `managed_components/` 下的文件**：这些由 Component Manager 自动管理，会被覆盖。
- **不要删除 `build/` 后直接用 `build`**：若删除整个 `build/` 目录，需重新执行 `set-target esp32s3` 和 `build`。
- **不要打破组件依赖方向**：`engine` 不应 include `ui` 的头文件；`utils` 应保持对高层模块的无知。

---

*本文档基于项目实际文件生成。若项目结构或配置发生变更，应同步更新本文件。*
