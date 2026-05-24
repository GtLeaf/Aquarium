@echo off
chcp 65001 >nul
echo ========================================
echo ESP32-S3-Touch-AMOLED-1.8 构建/烧录/监视工具
echo ========================================
echo.

set PYTHON=C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe
set CMAKE=C:\Espressif\tools\cmake\4.0.3\bin\cmake.exe
set ESPTOOL=D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\components\esptool_py\esptool\esptool.py
set IDF_MONITOR=D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf_monitor.py
set PORT=COM3
set BAUD=460800
set TARGET=esp32s3
set PROJECT_DIR=d:\Study\ESP32\hello_world
set BUILD_DIR=%PROJECT_DIR%\build

if "%1"=="build" goto do_build
if "%1"=="flash" goto do_flash
if "%1"=="monitor" goto do_monitor
if "%1"=="all" goto do_all

echo 用法: %0 [build ^| flash ^| monitor ^| all]
echo.
echo   build    - 构建固件（cmake 直接构建）
echo   flash    - 烧录固件到设备
echo   monitor  - 打开串口监视器查看日志
echo   all      - 构建 -> 烧录 -> 打开监视器
echo.
echo 当前配置:
echo   端口: %PORT%
echo   波特率: %BAUD%
echo   芯片: %TARGET%
echo   项目目录: %PROJECT_DIR%
echo.
goto end

:do_build
echo [1/1] 正在构建固件...
echo ========================================
cd /d %BUILD_DIR%
"%CMAKE%" --build .
if errorlevel 1 (
    echo 构建失败！
    goto end
)
echo 构建成功！
goto end

:do_flash
echo [1/2] 正在烧录固件...
echo ========================================
"%PYTHON%" "%ESPTOOL%" -p %PORT% -b %BAUD% --before default-reset --after hard-reset --chip %TARGET% write_flash --flash-mode dio --flash-freq 40m --flash-size 16MB 0x0 "%BUILD_DIR%\bootloader\bootloader.bin" 0x8000 "%BUILD_DIR%\partition_table\partition-table.bin" 0x10000 "%BUILD_DIR%\esp32s3_touch_amoled_18.bin"
if errorlevel 1 (
    echo 烧录失败！
    goto end
)
echo 烧录成功！
goto end

:do_monitor
echo [2/2] 正在打开串口监视器...
echo ========================================
echo 按 Ctrl+] 退出监视器
echo ========================================
"%PYTHON%" "%IDF_MONITOR%" -p %PORT% -b 115200 --toolchain-prefix xtensa-%TARGET%-elf- --make "'%PYTHON%' 'D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py'" --target %TARGET% "%BUILD_DIR%\esp32s3_touch_amoled_18.elf"
goto end

:do_all
call :do_build
if errorlevel 1 goto end
call :do_flash
if errorlevel 1 goto end
call :do_monitor
goto end

:end
