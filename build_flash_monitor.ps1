#requires -Version 5
<#
.SYNOPSIS
    ESP32-S3 项目构建、烧录、监视一体化脚本

.DESCRIPTION
    基于 ESP-IDF v6.0.1 + PowerShell 5 环境
    支持 build / flash / monitor / all / clean 操作

.PARAMETER Action
    要执行的动作：build, flash, monitor, all, clean, fullclean

.PARAMETER Port
    串口端口，默认 COM3

.PARAMETER Baud
    烧录波特率，默认 460800

.EXAMPLE
    .\build_flash_monitor.ps1 build
    .\build_flash_monitor.ps1 flash
    .\build_flash_monitor.ps1 monitor
    .\build_flash_monitor.ps1 all          # 构建+烧录+监视
    .\build_flash_monitor.ps1 clean        # 清理构建产物
    .\build_flash_monitor.ps1 fullclean    # 完全清理并重新配置
#>

param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("build", "flash", "monitor", "all", "clean", "fullclean")]
    [string]$Action,

    [string]$Port = "COM3",
    [int]$Baud = 460800
)

# ========================================
# 配置区域（根据实际安装路径修改）
# ========================================
$script:ESP_IDF_VERSION    = "6.0.1"
$script:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0.1\venv"
$script:PYTHON             = "C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe"
$script:IDF_PY             = "D:\softwareInstall\ESPIDF\v6.0.1\esp-idf\tools\idf.py"
$script:PROJECT_DIR        = "D:\Study\ESP32\hello_world"
$script:BUILD_DIR          = "$PROJECT_DIR\build"
$script:TARGET             = "esp32s3"

# 工具路径（加入 PATH）
$script:CMAKE_DIR  = "C:\Espressif\tools\cmake\4.0.3\bin"
$script:NINJA_DIR  = "C:\Espressif\tools\ninja\1.12.1"

# ========================================
# 初始化环境
# ========================================
function Initialize-Environment {
    # 设置环境变量
    $env:PATH = "$CMAKE_DIR;$NINJA_DIR;$env:PATH"
    $env:ESP_IDF_VERSION = $script:ESP_IDF_VERSION
    $env:IDF_PYTHON_ENV_PATH = $script:IDF_PYTHON_ENV_PATH

    # 验证关键文件存在
    $requiredFiles = @($PYTHON, $IDF_PY)
    foreach ($file in $requiredFiles) {
        if (-not (Test-Path $file)) {
            Write-Error "找不到文件: $file"
            Write-Error "请检查脚本顶部的配置路径是否正确"
            exit 1
        }
    }
}

# ========================================
# 杀掉残留 Python 进程
# ========================================
function Stop-ResidualPython {
    Write-Host "[检查] 杀掉可能占用 $Port 的残留 python 进程..." -ForegroundColor Yellow
    $processes = Get-Process -Name "python" -ErrorAction SilentlyContinue
    if ($processes) {
        $processes | Stop-Process -Force
        Start-Sleep -Seconds 1
        Write-Host "[OK] 已清理 $($processes.Count) 个残留进程" -ForegroundColor Green
    } else {
        Write-Host "[OK] 无残留进程" -ForegroundColor Green
    }
}

# ========================================
# 构建
# ========================================
function Invoke-Build {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  构建项目" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""

    Set-Location $PROJECT_DIR

    & $PYTHON $IDF_PY build
    if ($LASTEXITCODE -ne 0) {
        Write-Error "构建失败！"
        exit 1
    }

    Write-Host ""
    Write-Host "[OK] 构建成功！" -ForegroundColor Green
}

# ========================================
# 烧录
# ========================================
function Invoke-Flash {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  烧录固件 -> $Port @ ${Baud}bps" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""

    Set-Location $PROJECT_DIR

    & $PYTHON $IDF_PY -p $Port -b $Baud flash
    if ($LASTEXITCODE -ne 0) {
        Write-Error "烧录失败！"
        exit 1
    }

    Write-Host ""
    Write-Host "[OK] 烧录成功！" -ForegroundColor Green
}

# ========================================
# 监视
# ========================================
function Invoke-Monitor {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  串口监视器 -> $Port" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  按 Ctrl+] 退出监视器" -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""

    Set-Location $PROJECT_DIR

    # monitor 是交互式命令，直接调用不检查 exit code
    & $PYTHON $IDF_PY -p $Port monitor
}

# ========================================
# 清理
# ========================================
function Invoke-Clean {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  清理构建产物" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""

    Set-Location $PROJECT_DIR

    & $PYTHON $IDF_PY clean
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[OK] 清理完成" -ForegroundColor Green
    }
}

# ========================================
# 完全清理
# ========================================
function Invoke-FullClean {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  完全清理（删除 build 目录）" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""

    Set-Location $PROJECT_DIR

    & $PYTHON $IDF_PY fullclean
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[OK] 完全清理完成，下次构建将重新配置" -ForegroundColor Green
    }
}

# ========================================
# 主逻辑
# ========================================
Write-Host ""
Write-Host "========================================" -ForegroundColor Magenta
Write-Host "  ESP32-S3 构建/烧录/监视工具" -ForegroundColor Magenta
Write-Host "========================================" -ForegroundColor Magenta
Write-Host "  动作  : $Action" -ForegroundColor White
Write-Host "  端口  : $Port" -ForegroundColor White
Write-Host "  波特率: $Baud" -ForegroundColor White
Write-Host "  目标  : $TARGET" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Magenta

# 初始化环境
Initialize-Environment

# 根据动作执行
switch ($Action) {
    "build" {
        Invoke-Build
    }
    "flash" {
        Stop-ResidualPython
        Invoke-Flash
    }
    "monitor" {
        Stop-ResidualPython
        Invoke-Monitor
    }
    "all" {
        Invoke-Build
        Stop-ResidualPython
        Invoke-Flash
        Invoke-Monitor
    }
    "clean" {
        Invoke-Clean
    }
    "fullclean" {
        Invoke-FullClean
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Magenta
Write-Host "  完成" -ForegroundColor Magenta
Write-Host "========================================" -ForegroundColor Magenta
