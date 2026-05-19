@echo off
setlocal enabledelayedexpansion

:: 获取脚本目录并设置服务器路径
set "SCRIPT_DIR=%~dp0"
set "SERVER_PATH=%SCRIPT_DIR%scrcpy-server"

call :logd "脚本目录: %SCRIPT_DIR%"
call :logd "正在扫描设备..."

:: 获取设备列表并显示名称
set count=0
for /f "tokens=1" %%i in ('adb devices ^| findstr "device$"') do (
    set /a count+=1
    set "dev!count!=%%i"

    :: 获取设备型号名称，直接存入变量
    for /f "tokens=*" %%m in ('adb -s %%i shell getprop ro.product.model') do (
        set "model_name=%%m"
    )
    echo !count!. %%i - [!model_name!]
)

if %count%==0 (
    call :logd "未找到可用设备。"
    pause
    exit /b
)

:: 选择逻辑
if %count%==1 (
    set "target_device=!dev1!"
) else (
    echo 请选择设备序号:
    choice /c 123456789 /n /m "输入序号 (1-%count%): "
    set "idx=!errorlevel!"

    set "target_device="
    for /l %%k in (1,1,%count%) do (
        if !idx! equ %%k (
            set "target_device=!dev%%k!"
        )
    )
)

if "!target_device!"=="" (
    call :logd "错误：未正确选择设备。"
    pause
    exit /b
)

call :logd "已选择目标设备: !target_device!"

:: 推送
adb -s "!target_device!" push "%SERVER_PATH%" /data/local/tmp/scrcpy-server.jar
if %errorlevel% neq 0 (
    call :logd "推送失败。"
    pause
    exit /b
)

:: 启动
call :logd "启动 scrcpy..."
scrcpy --no-audio --video-codec=h264 --max-size=1280 --no-push -s "!target_device!"
exit /b

:logd
echo [%date:~5,10% %time:~0,8%] [LOG]: %~1
exit /b