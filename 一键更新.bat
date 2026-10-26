@echo off
setlocal
title Telegram Boost 一键自动更新器
cd /d "%~dp0"

if not exist "%~dp0update.ps1" (
    echo ======================================================
    echo [错误] 未在当前目录下找到 update.ps1 文件！
    echo 请确保 一键更新.bat 和 update.ps1 位于同一个文件夹中。
    echo ======================================================
    echo.
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0update.ps1"
set "PS_EXIT=%ERRORLEVEL%"

if %PS_EXIT% neq 0 (
    echo.
    echo [提示] 更新过程返回异常代码: %PS_EXIT%
    echo 如遇网络问题，请确认 v2rayN 代理已开启并允许系统代理。
    echo.
    pause
)
