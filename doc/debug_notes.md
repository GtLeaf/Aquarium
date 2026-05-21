# 像素生态缸 — 硬件调试排查记录

> 记录 ESP32-S3-Touch-AMOLED-1.8 开发板在 bring-up 过程中遇到的问题及解决方案。

---

## 1. 屏幕黑屏 / 不显示

### 现象
烧录后屏幕完全黑屏，没有任何显示。

### 根因
**I2C 引脚反了**导致 AXP2101 PMU 无法通信，进而导致触摸和显示屏没有供电。

### 解决
修正 `hal_i2c.c` 的引脚定义：
```c
// 错误
#define I2C_SDA_PIN     14
#define I2C_SCL_PIN     15

// 正确（ESP32-S3-Touch-AMOLED-1.8）
#define I2C_SDA_PIN     15
#define I2C_SCL_PIN     14
```

---

## 2. 屏幕颜色异常（青色/黄色/条纹）

### 现象
屏幕点亮但显示颜色不对：红/绿/蓝条纹一闪而过，然后画面变成纯青色或黄色。

### 根因
SH8601 AMOLED 驱动需要 **RGB565 SWAPPED** 格式（大端序），但 LVGL 默认使用 RGB565（小端序），导致颜色通道错位。

### 解决
在 `hal_lvgl.c` 中设置颜色格式：
```c
lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
```

---

## 3. 触摸无响应（I2C 读取成功但触摸点数量始终为 0）

### 现象
- 点击 START 按钮没有反应
- 监视器日志显示 `Touch data: [00 ff ff ff ff ff]`，触摸点数量始终为 0
- I2C 读取成功（无通信错误），但芯片不报告触摸事件

### 排查过程

#### 3.1 检查 I2C 设备地址
FT3168 标准地址是 `0x38`，扫描 I2C 总线确认设备存在。

#### 3.2 检查触摸芯片供电
通过 AXP2101 寄存器配置确认：
- **ALDO2** 必须开启（给 FT3168 触摸芯片供电）
- **ALDO4** 必须开启（给显示屏供电）

修正前仅开启了 ALDO1（麦克风），导致触摸芯片没电：
```c
// 错误：只开了 ALDO1
axp2101_write_byte(0x90, 0x01);

// 正确：ALDO1 + ALDO2 + ALDO4
axp2101_write_byte(0x90, 0x13);  // bit0=ALDO1, bit1=ALDO2, bit4=ALDO4
```

#### 3.3 检查触摸复位
FT3168 的复位引脚 **不是直接连到 GPIO**，而是通过 **PCA9554 I/O 扩展器的 P2 引脚**控制。

添加 PCA9554 复位序列：
```c
// PCA9554 地址 0x20
// P2 = 触摸复位（低电平复位，高电平释放）
hal_i2c_write_byte(0x20, 0x03, 0x00);  // 配置 P0-P2 为输出
hal_i2c_write_byte(0x20, 0x01, 0x00);  // P2=0（复位低电平）
vTaskDelay(50);
hal_i2c_write_byte(0x20, 0x01, 0x04);  // P2=1（释放复位）
vTaskDelay(100);
```

#### 3.4 检查芯片工作模式
某些批次的 FT3168 复位后默认处于 Factory/Monitor 模式，需要显式设置为 Normal Mode：
```c
// 设置 Normal Mode
hal_i2c_write_byte(0x38, 0x00, 0x00);  // 寄存器 0x00 = 0x00 (Normal)
```

#### 3.5 验证芯片 ID
读取芯片 ID 验证通信正常：
```c
uint8_t chip_id;
hal_i2c_read_byte(0x38, 0xA3, &chip_id);  // 预期值 0x64
```

#### 3.6 坐标映射修正
该批次的 FT3168 直接输出屏幕分辨率坐标（368×448），不需要再映射：
```c
// 错误：再次映射导致坐标错误
*x = (raw_x * DISPLAY_WIDTH) / 2048;
*y = (raw_y * DISPLAY_HEIGHT) / 2048;

// 正确：直接使用原始坐标
*x = raw_x;
*y = raw_y;
```

### 最终修复代码位置
- `components/eco_hal/hal_i2c.c` — I2C 引脚修正
- `components/eco_hal/hal_pmu.c` — AXP2101 供电配置（ALDO1/2/4）
- `components/eco_hal/hal_touch.c` — FT3168 复位、初始化、坐标映射

---

## 4. 栈溢出导致重启

### 现象
屏幕空白，串口日志显示 `A stack overflow in task main has been detected`，然后设备重启。

### 根因
`main` 任务的栈太小（默认 3584 字节），UI 初始化过程中使用了大量栈空间。

### 解决
在 `sdkconfig.defaults` 中增加主任务栈大小：
```
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
```

---

## 5. 构建环境配置

### 现象
直接运行 `idf.py build` 报错 `"cmake" must be available on the PATH`。

### 根因
Windows 终端没有正确加载 ESP-IDF 环境变量。

### 解决
必须通过 **ESP-IDF PowerShell**（桌面快捷方式）或手动运行 `export.ps1` 来激活环境：
```powershell
# 方法1：桌面快捷方式 "IDF_v6.0.1_Powershell"
# 方法2：手动导入
. D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\export.ps1
idf.py build
```

---

## 6. 关键硬件信息速查

| 组件 | I2C 地址 | 供电 LDO | 备注 |
|------|---------|---------|------|
| AXP2101 | 0x34 | — | PMU 电源管理 |
| PCA9554 | 0x20 | — | I/O 扩展器，P2 控制触摸复位 |
| FT3168 | 0x38 | ALDO2 | 电容触摸芯片 |
| PCF85063 | 0x51 | ALDO1 | RTC 实时时钟 |
| QMI8658 | 0x6B | ALDO1 | 六轴 IMU |
| ES8311 | 0x18 | — | I2S 音频编解码 |

### AXP2101 LDO 配置
- **ALDO1** (0x92) → 3.3V，给 RTC、IMU 供电
- **ALDO2** (0x93) → 3.3V，给 **FT3168 触摸**供电
- **ALDO4** (0x95) → 3.3V，给 **显示屏、TF 卡**供电

### I2C 引脚
- **SDA** = GPIO15
- **SCL** = GPIO14

### QSPI 显示引脚
- **CS** = GPIO12
- **CLK** = GPIO11
- **D0** = GPIO4
- **D1** = GPIO5
- **D2** = GPIO6
- **D3** = GPIO7
- **RST** = GPIO17

---

*记录时间：2026-05-21*
