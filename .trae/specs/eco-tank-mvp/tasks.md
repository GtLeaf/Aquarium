# Tasks

## Phase 1: 项目骨架与构建系统（W1 前半周）

- [ ] Task 1: 重构 CMake 构建系统，集成 LVGL 9.x 与 LittleFS
  - [ ] SubTask 1.1: 修改根目录 CMakeLists.txt，添加 idf_component_register 与外部组件依赖
  - [ ] SubTask 1.2: 创建 components/ 目录结构（hal/ ui/ engine/ save/ utils/）
  - [ ] SubTask 1.3: 配置 sdkconfig.defaults（LVGL 帧缓冲、PSRAM、NVS 分区）
  - [ ] SubTask 1.4: 验证 `idf.py build` 通过，生成 hello_world.elf

- [ ] Task 2: 硬件抽象层（HAL）基础
  - [ ] SubTask 2.1: 实现 hal_display（QSPI AMOLED 初始化，368×448 分辨率）
  - [ ] SubTask 2.2: 实现 hal_touch（FT3168 电容触控，单点/滑动/长按识别）
  - [ ] SubTask 2.3: 实现 hal_rtc（PCF85063 读写，SNTP 校时）
  - [ ] SubTask 2.4: 实现 hal_pmu（AXP2101 电池电量读取，充电状态检测）
  - [ ] SubTask 2.5: 实现 hal_imu（QMI8658 初始化，摇晃检测算法）

## Phase 2: 数据模型与配置表（W1 后半周）

- [ ] Task 3: 核心数据结构定义
  - [ ] SubTask 3.1: 定义 Environment / Creature / Species / GameSave 结构体（spec.md §7.2 / §11.3.2 / §11.4.2）
  - [ ] SubTask 3.2: 定义 Event 结构体（24 个事件，含 precondition 表达式）
  - [ ] SubTask 3.3: 将 species.csv / events.csv 编译为 C 常量数组（Python 代码生成脚本）
  - [ ] SubTask 3.4: 实现物种数据库查询 API（按 ID / 营养级 / 稀有度过滤）

- [ ] Task 4: 存档系统（Save System）
  - [ ] SubTask 4.1: 实现 NVS 读写封装（含 CRC32 校验）
  - [ ] SubTask 4.2: 实现 GameSave 序列化/反序列化
  - [ ] SubTask 4.3: 实现自动存档（5 分钟定时器）与手动存档
  - [ ] SubTask 4.4: 实现存档导入/导出（JSON/LittleFS 文件）

## Phase 3: 生态心跳引擎（W2）

- [ ] Task 5: 环境层模拟
  - [ ] SubTask 5.1: 实现阳光/营养/氧气更新逻辑（白天/夜晚循环）
  - [ ] SubTask 5.2: 实现 Logistic 藻类生长公式（避免爆缸）
  - [ ] SubTask 5.3: 实现充电触发"营养雨"事件

- [ ] Task 6: 生物 AI 与行为
  - [ ] SubTask 6.1: 实现 L1 生产者生长（藻类/水草生物量）
  - [ ] SubTask 6.2: 实现 L2/L3/L4 觅食 AI（最近目标搜索 + 接触判定）
  - [ ] SubTask 6.3: 实现个体成长（growth_pts 累积 → size + stage 升级）
  - [ ] SubTask 6.4: 实现繁殖产卵（同种成体 + 饱食度 > 70 + 数量上限）
  - [ ] SubTask 6.5: 实现柔性死亡（睡眠态 → 温柔消失 + 营养回收）
  - [ ] SubTask 6.6: 实现稳态修复（藻类爆发自动清理、鱼太多自动停止繁殖）

- [ ] Task 7: 离线产出系统
  - [ ] SubTask 7.1: 实现离线时间计算（RTC 时间差）
  - [ ] SubTask 7.2: 实现光合币公式与上限检查
  - [ ] SubTask 7.3: 实现离线收益弹窗动画

## Phase 4: 随机事件系统（W3）

- [ ] Task 8: 事件触发器引擎
  - [ ] SubTask 8.1: 实现 trigger_type 分发器（scheduled / weather_api / charging / interaction / low_prob / time）
  - [ ] SubTask 8.2: 实现每日 cap 与冷却时间检查
  - [ ] SubTask 8.3: 实现 precondition DSL 解析器（支持 && / || / == / >= / <=）

- [ ] Task 9: 事件奖励与图鉴
  - [ ] SubTask 9.1: 实现奖励池随机抽取（按权重）
  - [ ] SubTask 9.2: 实现图鉴位图管理（uint64 collected_species）
  - [ ] SubTask 9.3: 实现长尾物种解锁逻辑（百日守护 / 进化 / 满级+雷暴）
  - [ ] SubTask 9.4: 实现新手周（D1-D7）与半月扶持（D8-D14）保底机制

## Phase 5: UI 渲染系统（W4）

- [ ] Task 10: LVGL 基础与主界面
  - [ ] SubTask 10.1: 初始化 LVGL（显示缓冲、输入设备注册）
  - [ ] SubTask 10.2: 实现主界面（TANK_VIEW）：状态栏 + 缸景 + 触发热区
  - [ ] SubTask 10.3: 实现状态栏（阳光/营养/氧气图标 + 进度填充 + 时间 + 电量）
  - [ ] SubTask 10.4: 实现生物精灵渲染（像素图按 size 缩放、4 stage 切换、动画帧）

- [ ] Task 11: 弹窗与图鉴界面
  - [ ] SubTask 11.1: 实现事件弹窗（标题 + 预览图 + 描述 + 打开/稍后按钮）
  - [ ] SubTask 11.2: 实现图鉴页（3×4 网格、6 分类页签、已解锁/未解锁状态）
  - [ ] SubTask 11.3: 实现属性卡弹窗（鱼大图 + 4 状态条 + 喂食/抚摸/放生按钮）
  - [ ] SubTask 11.4: 实现离线收益弹窗动画

- [ ] Task 12: 桌面伴侣模式
  - [ ] SubTask 12.1: 实现 Ambient Mode（60 秒无操作自动进入）
  - [ ] SubTask 12.2: 实现时钟渲染（左下角时间、右上角电量）
  - [ ] SubTask 12.3: 实现 1 FPS 渲染与低功耗切换
  - [ ] SubTask 12.4: 实现点击唤醒（返回 TANK_VIEW）

## Phase 6: 多模态交互与音效（W5）

- [ ] Task 13: 传感器交互
  - [ ] SubTask 13.1: 实现摇晃喂食（IMU 摇晃检测 → 水波 + 生物受惊）
  - [ ] SubTask 13.2: 实现倾斜重力（水位倾斜，鱼游向低处）
  - [ ] SubTask 13.3: 实现吹气涟漪（麦克风检测 → 水面涟漪）
  - [ ] SubTask 13.4: 实现触摸交互（点击撒食、长按摸头、滑动擦缸）

- [ ] Task 14: 音频系统
  - [ ] SubTask 14.1: 初始化 ES8311 I²S 音频驱动
  - [ ] SubTask 14.2: 实现音效播放 API（事件音效、生物音效、环境白噪音）
  - [ ] SubTask 14.3: 实现背景白噪音循环播放

## Phase 7: 网络与天气联动（W6）

- [ ] Task 15: Wi-Fi 与天气
  - [ ] SubTask 15.1: 实现 Wi-Fi STA 连接管理
  - [ ] SubTask 15.2: 实现 HTTPS 天气 API 拉取（OpenWeatherMap 等）
  - [ ] SubTask 15.3: 实现天气事件映射（雨/雷暴/雪触发对应事件）
  - [ ] SubTask 15.4: 实现节气计算（24 节气日期算法）

- [ ] Task 16: 性能优化与验收
  - [ ] SubTask 16.1: 内存优化（确保 RAM < 4MB，PSRAM 帧缓冲）
  - [ ] SubTask 16.2: 功耗优化（Ambient Mode < 30mA）
  - [ ] SubTask 16.3: 帧率优化（前台 30 FPS 稳定）
  - [ ] SubTask 16.4: 烧屏防护（像素偏移、定时反色）
  - [ ] SubTask 16.5: 完整集成测试（食物链闭环、事件触发、存档读写）

# Task Dependencies

- Task 2 (HAL) 依赖 Task 1 (构建系统)
- Task 3 (数据结构) 依赖 Task 1 (构建系统)
- Task 4 (存档) 依赖 Task 3 (数据结构)
- Task 5 (环境) 依赖 Task 3 (数据结构)
- Task 6 (生物 AI) 依赖 Task 5 (环境)
- Task 7 (离线) 依赖 Task 4 (存档) + Task 6 (生物)
- Task 8 (事件触发) 依赖 Task 3 (数据结构) + Task 4 (存档)
- Task 9 (奖励图鉴) 依赖 Task 8 (事件触发) + Task 6 (生物)
- Task 10 (LVGL UI) 依赖 Task 2 (HAL 显示/触摸)
- Task 11 (弹窗图鉴) 依赖 Task 10 (LVGL 基础)
- Task 12 (伴侣模式) 依赖 Task 10 (LVGL 基础) + Task 6 (生物)
- Task 13 (传感器) 依赖 Task 2 (HAL IMU/麦克)
- Task 14 (音频) 依赖 Task 2 (HAL 音频)
- Task 15 (网络) 依赖 Task 2 (HAL Wi-Fi)
- Task 16 (优化) 依赖所有前述任务

# 可并行任务组

- **Group A**: Task 1 + Task 2（硬件无关部分可并行）
- **Group B**: Task 3 + Task 4（数据与存储）
- **Group C**: Task 5 + Task 6 + Task 7（生态引擎内部）
- **Group D**: Task 8 + Task 9（事件系统）
- **Group E**: Task 10 + Task 11 + Task 12（UI 渲染）
- **Group F**: Task 13 + Task 14 + Task 15（硬件交互）
