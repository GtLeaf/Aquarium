# 构建与烧录指南（Windows + ESP-IDF v6.0.1）

> 本文档记录本项目的构建命令，供 AI 智能体参考。
> 项目：`d:\Study\ESP32\hello_world`
> 芯片：ESP32-S3（Waveshare ESP32-S3-Touch-AMOLED-1.8）
> ESP-IDF：v6.0.1（安装在 `D:\softwareInstall\ESPIDF\v6.0.1\esp-idf`）
> Shell 环境：**PowerShell 5**（非 MSYS2/bash）

---

## 1. Shell 环境说明

当前终端为 **PowerShell 5**，路径使用 Windows 格式（如 `C:\Espressif\...`）。

**关键环境变量**（每次执行 idf.py 前需设置）：

```powershell
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;$env:PATH"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
```

> 如果不设置这些变量，`idf.py` 会报错：找不到 `cmake`、`ninja` 或 `ESP_IDF_VERSION` 未定义。

---

## 2. 完整构建命令

```powershell
# 设置环境变量并构建
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;$env:PATH"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py build
```

### 输出示例

```
Executing action: all (aliases: build)
Running ninja in directory D:\Study\ESP32\hello_world\build
Executing "ninja all"...
[1/4] ... esp32s3_touch_amoled_18.bin
esp32s3_touch_amoled_18.bin binary size 0x92b20 bytes. Smallest app partition is 0x200000 bytes. 0x16d4e0 bytes (71%) free.
[4/4] Completed 'bootloader'
Project build complete. To flash, run: idf.py flash
```

### 首次构建（无 build 目录时）

如果 `build/` 目录不存在或被删除，需要先执行 CMake 配置：

```powershell
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;$env:PATH"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py fullclean
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py set-target esp32s3
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py build
```

> 通常 `build/` 目录已存在且包含 `CMakeCache.txt`，直接用 `build` 即可增量编译。

---

## 3. 烧录命令

使用 `idf.py flash`（推荐，与构建命令一致的环境变量）：

```powershell
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;$env:PATH"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py -p COM3 -b 460800 flash
```

### 如果 COM3 被占用

ESP-IDF 的 monitor 进程（`python.exe`）有时会残留并占用 COM3，导致烧录失败。

**先杀掉残留进程**：

```powershell
powershell -Command "Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force"
```

然后再执行烧录。

---

## 4. 串口监视命令

使用 `idf.py monitor`（波特率自动为 115200）：

```powershell
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;$env:PATH"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py -p COM3 monitor
```

- 按 **Ctrl+]** 退出监视器
- 自动触发芯片复位并输出启动日志

---

## 5. 一键构建+烧录+监视（完整流程）

```powershell
# 1. 设置环境变量
$env:PATH = "C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;$env:PATH"
$env:ESP_IDF_VERSION = "6.0.1"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
$PYTHON = "C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe"
$IDF_PY = "D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py"

# 2. 杀掉可能占用 COM3 的残留进程
powershell -Command "Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force"

# 3. 构建
& $PYTHON $IDF_PY build

# 4. 烧录
& $PYTHON $IDF_PY -p COM3 -b 460800 flash

# 5. 监视（如需后台运行，加 -Blocking:$false）
& $PYTHON $IDF_PY -p COM3 monitor
```

---

## 6. 常见错误

| 错误 | 原因 | 解决 |
|---|---|---|
| `The IDF_PYTHON_ENV_PATH is missing` | 未设置 `IDF_PYTHON_ENV_PATH` | 按第 1 节设置环境变量 |
| `cmake must be available on the PATH` | `cmake` 不在 PATH 中 | 将 `C:\Espressif\tools\cmake\4.0.3\bin` 加入 PATH |
| `FileNotFoundError: ninja` | `ninja` 不在 PATH 中 | 将 `C:\Espressif\tools\ninja\1.12.1` 加入 PATH |
| `TypeError: expected string or bytes-like object, got 'NoneType'` | `ESP_IDF_VERSION` 未设置 | 设置 `$env:ESP_IDF_VERSION = "6.0.1"` |
| `Access to the port 'COM3' is denied` | 残留 `python.exe` 占用串口 | 用 PowerShell `Stop-Process` 杀掉 |
| 构建无变化 | 未修改源文件 | 确认修改了 `.c` 文件而非头文件缓存问题 |

---

## 7. 文件路径速查

| 文件/目录 | 路径 |
|---|---|
| 项目根目录 | `d:\Study\ESP32\hello_world` |
| 构建目录 | `d:\Study\ESP32\hello_world\build` |
| 固件二进制 | `build\esp32s3_touch_amoled_18.bin` |
| Python 虚拟环境 | `C:\Espressif\tools\python\v6.0.1\venv` |
| Python 可执行文件 | `C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe` |
| CMake 可执行文件 | `C:\Espressif\tools\cmake\4.0.3\bin\cmake.exe` |
| Ninja 可执行文件 | `C:\Espressif\tools\ninja\1.12.1\ninja.exe` |
| idf.py 脚本 | `D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py` |
| 烧录脚本（旧） | `flash_and_monitor.cmd` |
| ESP-IDF 根目录 | `D:\softwareInstall\ESPIDF\v6.0.1\esp-idf` |

---

*本文档由 AI 助手生成，用于后续智能体快速构建和烧录。*
