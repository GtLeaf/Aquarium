# 像素生态缸（治愈放置版）MVP 技术方案 Spec

## Why

基于 PRD v0.4 文档，需要在 ESP32-S3-Touch-AMOLED-1.8（368×448）硬件上实现一个像素风桌面生态缸游戏。当前项目为 ESP-IDF hello_world 示例，需要从零搭建完整的游戏框架、生态引擎、UI 系统和硬件抽象层。

## What Changes

- **新增** 完整的游戏引擎框架（状态机、事件系统、生态心跳）
- **新增** LVGL 9.x UI 渲染层（主界面、事件弹窗、图鉴、属性卡）
- **新增** 硬件抽象层（触摸屏、IMU、RTC、音频、Wi-Fi）
- **新增** 生态系统模拟（4 层食物链、个体成长、繁殖、死亡回收）
- **新增** 随机事件系统（24 个事件、触发条件、奖励池）
- **新增** 存档系统（NVS + LittleFS 双写保护）
- **新增** 桌面伴侣模式（Ambient Mode、时钟挂件）

## Impact

- Affected specs: 游戏循环、生态模拟、UI 渲染、硬件交互、数据持久化
- Affected code: 全部新增，基于现有 ESP-IDF 5.x + CMake 构建系统

## ADDED Requirements

### Requirement: 项目构建系统
The system SHALL 使用 ESP-IDF 5.x 构建，支持 ESP32-S3 目标，集成 LVGL 9.x、LittleFS、NVS 组件。

#### Scenario: 成功构建
- **WHEN** 执行 `idf.py build`
- **THEN** 成功编译并链接所有游戏模块，生成可烧录固件

### Requirement: 硬件抽象层 (HAL)
The system SHALL 封装所有硬件访问，提供统一 API：触摸屏（FT3168）、IMU（QMI8658）、RTC（PCF85063）、音频（ES8311）、电池（AXP2101）。

#### Scenario: 硬件初始化
- **WHEN** 系统启动
- **THEN** 所有硬件模块按依赖顺序初始化，失败时进入降级模式

#### Scenario: 触摸事件
- **WHEN** 用户点击屏幕
- **THEN** HAL 上报坐标 + 手势类型（单击/滑动/长按）到 UI 层

#### Scenario: IMU 摇晃检测
- **WHEN** 用户摇晃设备
- **THEN** HAL 上报摇晃强度，触发水波 + 生物受惊效果

### Requirement: 游戏状态机
The system SHALL 维护 5 个核心状态：BOOT → TITLE → TANK_VIEW → EVENT_POPUP → AMBIENT_MODE，支持状态间合法跳转。

#### Scenario: 状态跳转
- **WHEN** TANK_VIEW 状态下 60 秒无操作
- **THEN** 自动切换到 AMBIENT_MODE（时钟挂件）

#### Scenario: 唤醒
- **WHEN** AMBIENT_MODE 下用户点击屏幕
- **THEN** 切换回 TANK_VIEW

### Requirement: 生态心跳引擎
The system SHALL 以 1 Hz 频率执行生态循环：环境更新 → L1 生长 → L2/L3/L4 觅食 → 个体成长 → 繁殖产卵 → 衰减/睡眠 → 死亡分解 → 玩家事件回收。

#### Scenario: 环境更新
- **WHEN** 每 1 秒心跳触发
- **THEN** 更新阳光（白天+1/min，夜晚-0.5/min）、营养、氧气值

#### Scenario: 觅食 AI
- **WHEN** L2/L3/L4 生物 hunger > 阈值
- **THEN** 寻找半径 R 内可食目标，加速接近，接触判定后进食

#### Scenario: 成长触发
- **WHEN** 生物 growth_pts 达阈值
- **THEN** size +5，触发"长大"动画，stage 可能升级

#### Scenario: 柔性死亡
- **WHEN** 生物 hunger > 95 持续 30 分钟
- **THEN** 进入睡眠态（沉底 + 鼾泡），24h 后温柔消失化为营养

### Requirement: 生物渲染系统
The system SHALL 支持 30 种物种的像素精灵渲染，按 size/100 缩放，4 套 stage 精灵（幼体/亚成体/成体/巨型），同屏动画对象上限 24 个。

#### Scenario: 主界面渲染
- **WHEN** TANK_VIEW 状态
- **THEN** 渲染缸景背景 + 所有活跃生物（位置、朝向、动画帧）+ 状态栏

### Requirement: 随机事件系统
The system SHALL 支持 24 个事件，按 trigger_type（scheduled/weather_api/charging/interaction/low_prob/time）检查触发条件，每日 cap 控制上限。

#### Scenario: 海鸥来访
- **WHEN** 每日首次打开且满足冷却
- **THEN** 顶部出现海鸥剪影预告，点击弹出事件卡片，打开后播放礼物盒动画

#### Scenario: 新手周礼包
- **WHEN** D1-D7 每天
- **THEN** 触发 newbie_week 事件，保底新种 55% 概率

### Requirement: 离线产出系统
The system SHALL 计算离线光合币：在线生物数 × 系数 × 时长(h)，上限按缸等级递增，返回时弹出收益动画。

#### Scenario: 离线收益
- **WHEN** 用户重新打开设备
- **THEN** 对比 last_offline_ts 与当前 RTC 时间，计算并发放离线收益

### Requirement: 存档系统
The system SHALL 使用 NVS 存储 GameSave 结构（含环境、生物数组、图鉴位图、事件计数），每 5 分钟自动持久化，支持导入/导出。

#### Scenario: 自动存档
- **WHEN** 每 5 分钟或状态重大变更
- **THEN** 写入 NVS，失败时重试 3 次

#### Scenario: 读档
- **WHEN** 系统启动
- **THEN** 从 NVS 读取 GameSave，校验 CRC，失败时初始化默认存档

### Requirement: UI 系统
The system SHALL 基于 LVGL 实现所有界面，遵循 368×448 单屏信息密度规范：中文最小 14px、单行 ≤22 字、点击元素 ≤5 个、触控目标 ≥40×40px。

#### Scenario: 主界面
- **WHEN** TANK_VIEW 状态
- **THEN** 顶部状态栏（阳光/营养/氧气 + 时间 + 电量），中部缸景，底部触发热区

#### Scenario: 事件弹窗
- **WHEN** 事件触发
- **THEN** 50% 透明黑遮罩 + 标题 + 预览图 + 描述 ≤2 行 + 打开/稍后按钮

#### Scenario: 图鉴页
- **WHEN** 用户进入图鉴
- **THEN** 3×4 网格分页，按 L1/L2/L3/L4/访客/装饰 6 个页签切换

### Requirement: 桌面伴侣模式
The system SHALL 在 60 秒无操作后进入 Ambient Mode：左下角时间、右上角电量、1-2 只主鱼像素动画、1 FPS 渲染、AMOLED 黑色像素不发光。

#### Scenario: 进入伴侣模式
- **WHEN** TANK_VIEW 下 60 秒无触摸/摇晃
- **THEN** 切换到 AMBIENT_MODE，降低刷新率到 1 FPS

### Requirement: 性能预算
The system SHALL 满足：RAM < 4MB（含 LVGL Buffer）、Flash < 8MB、桌面伴侣待机功耗 < 30mA、渲染 30 FPS（前台）/ 1 FPS（伴侣）。

#### Scenario: 内存监控
- **WHEN** 系统运行中
- **THEN** 定期检查堆内存，低于阈值时触发 GC 或降级

## MODIFIED Requirements

无（基于全新项目）

## REMOVED Requirements

无
