# 构建与烧录指南（Windows + ESP-IDF v6.0.1）

> 本文档记录本项目的构建命令，供 AI 智能体参考。
> 项目：`d:\Study\ESP32\hello_world`
> 芯片：ESP32-S3（Waveshare ESP32-S3-Touch-AMOLED-1.8）
> ESP-IDF：v6.0.1（安装在 `D:\softwareInstall\ESPIDF\v6.0.1\esp-idf`）

---

## 1. 两个脚本的区别

项目中提供两个烧录/构建脚本，根据执行环境选择：

| 脚本 | 环境 | 支持的操作 | 构建能力 |
|---|---|---|---|
| `flash_and_monitor.cmd` | CMD / 批处理 | `flash` / `monitor` / `all` | ❌ 不支持 build |
| `build_flash_monitor.ps1` | **PowerShell** | `build` / `flash` / `monitor` / `all` / `clean` / `fullclean` | ✅ 支持 build |

**推荐**：在 PowerShell 环境下优先使用 `build_flash_monitor.ps1`，功能更完整。

---

## 2. PowerShell 环境（推荐）：build_flash_monitor.ps1

此脚本调用 `idf.py`，需要在 **原生 PowerShell / CMD** 中执行，不经过 MSYS bash。

```powershell
# 构建（增量编译）
.\build_flash_monitor.ps1 build

# 烧录（自动杀掉占用 COM3 的残留 python 进程）
.\build_flash_monitor.ps1 flash

# 串口监视器
.\build_flash_monitor.ps1 monitor

# 构建 → 烧录 → 监视（一条龙）
.\build_flash_monitor.ps1 all

# 清理构建产物
.\build_flash_monitor.ps1 clean

# 完全清理（删除 build 目录，下次重新配置）
.\build_flash_monitor.ps1 fullclean
```

### 参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| `Action` | 必填 | `build` / `flash` / `monitor` / `all` / `clean` / `fullclean` |
| `Port` | `COM3` | 串口 |
| `Baud` | `460800` | 烧录波特率 |

示例：指定端口和波特率
```powershell
.\build_flash_monitor.ps1 flash -Port COM4 -Baud 921600
```

---

## 3. MSYS Bash 环境（当前 Shell）：cmake 直接构建

当前智能体运行在 MSYS2/bash 中，直接执行 `idf.py build` 会报错：

```
MSys/Mingw is no longer supported. Please follow the getting started guide...
```

**原因**：ESP-IDF v6.0.1 的 `idf.py` 检测到 MSYS 环境后拒绝执行。

**解决方案**：绕过 `idf.py`，直接用 `cmake` 构建（ESP-IDF 本质上是 CMake 项目）。

### 构建命令

```bash
# 进入 build 目录
cd /d/Study/ESP32/hello_world/build

# 直接用 cmake 构建（不依赖 idf.py）
/c/Espressif/tools/cmake/4.0.3/bin/cmake.exe --build .
```

### 输出示例

```
[9/9] ...
esptool v5.3.dev3
Creating ESP32-S3 image...
Successfully created ESP32-S3 image.
Generated D:/Study/ESP32/hello_world/build/esp32s3_touch_amoled_18.bin
esp32s3_touch_amoled_18.bin binary size 0x928a0 bytes. Smallest app partition is 0x200000 bytes. 0x16d760 bytes (71%) free.
```

### 首次构建（无 build 目录时）

如果 `build/` 目录不存在或被删除，需要先执行 CMake 配置：

```bash
cd /d/Study/ESP32/hello_world
mkdir -p build
cd build
/c/Espressif/tools/cmake/4.0.3/bin/cmake.exe .. -G Ninja -D IDF_TARGET=esp32s3
/c/Espressif/tools/cmake/4.0.3/bin/cmake.exe --build .
```

> 通常 `build/` 目录已存在且包含 `CMakeCache.txt`，直接用 `--build .` 即可增量编译。

---

## 4. 烧录命令（两种环境通用）

### 方法 A：flash_and_monitor.cmd（CMD / MSYS bash 都可用）

```bash
cd /d/Study/ESP32/hello_world
./flash_and_monitor.cmd flash
```

### 方法 B：build_flash_monitor.ps1（PowerShell）

```powershell
cd D:\Study\ESP32\hello_world
.\build_flash_monitor.ps1 flash
```

### 如果 COM3 被占用

ESP-IDF 的 monitor 进程（`python.exe`）有时会残留并占用 COM3，导致烧录失败。

**先杀掉残留进程**：

```bash
# MSYS bash
powershell -Command "Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep -Seconds 1"
```

```powershell
# PowerShell（build_flash_monitor.ps1 已内置此逻辑）
Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force
```

然后再执行烧录。

---

## 5. 串口日志读取（无 TTY 的替代方案）

`idf.py monitor` 需要 TTY，在后台/脚本中无法使用。改用 PowerShell 读取：

```bash
powershell -Command "
\$port = New-Object System.IO.Ports.SerialPort 'COM3', 115200, None, 8, One
\$port.DtrEnable = \$false
\$port.RtsEnable = \$true
\$port.Open()
Start-Sleep -Milliseconds 100
\$port.RtsEnable = \$false
Start-Sleep -Milliseconds 800
\$start = Get-Date
while (((Get-Date) - \$start).TotalSeconds -lt 15) {
    if (\$port.BytesToRead -gt 0) {
        \$data = \$port.ReadExisting()
        Write-Host -NoNewline \$data
    }
    Start-Sleep -Milliseconds 50
}
\$port.Close()
"
```

- 波特率：115200
- 读取时长：15 秒（可调整）
- 自动触发 RTS 复位，无需手动按 RESET

---

## 6. 一键完整流程（MSYS Bash 环境）

```bash
# 1. 进入项目目录
cd /d/Study/ESP32/hello_world

# 2. 杀掉可能占用 COM3 的残留进程
powershell -Command "Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep -Seconds 1"

# 3. 构建（cmake 直接构建，绕过 idf.py）
cd build
/c/Espressif/tools/cmake/4.0.3/bin/cmake.exe --build .

# 4. 烧录
cd /d/Study/ESP32/hello_world
./flash_and_monitor.cmd flash
```

---

## 7. 一键完整流程（PowerShell 环境）

```powershell
# 构建 + 烧录（监视器需要交互式 TTY，不适合后台执行）
cd D:\Study\ESP32\hello_world
.\build_flash_monitor.ps1 all
```

---

## 8. 常见错误

| 错误 | 原因 | 解决 |
|---|---|---|
| `MSys/Mingw is no longer supported` | `idf.py` 检测到 MSYS 环境 | 改用 `cmake.exe --build .` 或切换到 PowerShell |
| `Access to the port 'COM3' is denied` | 残留 `python.exe` 占用串口 | 用 PowerShell `Stop-Process` 杀掉 |
| `No such file or directory` (cmake) | CMake 路径不对 | 确认路径 `/c/Espressif/tools/cmake/4.0.3/bin/cmake.exe` |
| 构建无变化 | 未修改源文件 | 确认修改了 `.c` 文件而非头文件缓存问题 |
| `idf.py: command not found` | 未激活 ESP-IDF 环境 | 用 `build_flash_monitor.ps1` 或在 PowerShell 中执行 |

---

## 9. 文件路径速查

| 文件/目录 | 路径 |
|---|---|
| 项目根目录 | `d:\Study\ESP32\hello_world` |
| 构建目录 | `d:\Study\ESP32\hello_world\build` |
| 固件二进制 | `build\esp32s3_touch_amoled_18.bin` |
| CMake 可执行文件 | `C:\Espressif\tools\cmake\4.0.3\bin\cmake.exe` |
| 烧录脚本 (CMD) | `flash_and_monitor.cmd` |
| 构建脚本 (PowerShell) | `build_flash_monitor.ps1` |
| ESP-IDF 根目录 | `D:\softwareInstall\ESPIDF\v6.0.1\esp-idf` |
| Python venv | `C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe` |

---

*本文档由 AI 助手生成，用于后续智能体快速构建和烧录。*
