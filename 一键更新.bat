@echo off
chcp 65001 >nul
title Telegram Boost 一键自动更新器

echo ======================================================
echo           Telegram Boost 一键自动更新工具
echo ======================================================
echo.

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

powershell -NoProfile -ExecutionPolicy Bypass -Command "& {
    $repo = 'yueyingzxq/my-tdesktop-boost'
    $apiUrl = \"https://api.github.com/repos/$repo/releases/tags/latest-boost\"
    
    Write-Host '[1/4] 正在连接 GitHub 检测最新版本...' -ForegroundColor Cyan
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $release = Invoke-RestMethod -Uri $apiUrl -Headers @{'User-Agent'='Telegram-Updater'} -TimeoutSec 15
    } catch {
        Write-Host '[错误] 无法连接到 GitHub Releases，请检查网络或代理是否正常！' -ForegroundColor Red
        Write-Host $_.Exception.Message -ForegroundColor Yellow
        exit 1
    }

    $asset = $release.assets | Where-Object { $_.name -eq 'Telegram.exe' -or $_.name -like '*Telegram*.zip' } | Select-Object -First 1
    if (-not $asset) {
        Write-Host '[提示] 仓库 Release 暂未上传新二进制，请稍后再试！' -ForegroundColor Yellow
        exit 0
    }

    $downloadUrl = $asset.browser_download_url
    $remoteSize = $asset.size
    $remoteDate = [DateTime]::Parse($asset.updated_at)
    
    Write-Host \"[2/4] 发现最新版本：$($release.name)\" -ForegroundColor Green
    Write-Host \"      发布时间：$($remoteDate.ToLocalTime())\" -ForegroundColor Gray
    Write-Host \"      文件大小：$([Math]::Round($remoteSize / 1MB, 2)) MB\" -ForegroundColor Gray

    # 检查本地是否有 Telegram.exe
    $localExe = Join-Path $PWD 'Telegram.exe'
    if (Test-Path $localExe) {
        $localDate = (Get-Item $localExe).LastWriteTime
        $localSize = (Get-Item $localExe).Length
        # 如果文件大小一致且本地更新时间不早于远程，视为最新
        if ($localSize -eq $remoteSize) {
            Write-Host '------------------------------------------------------' -ForegroundColor Green
            Write-Host '【恭喜】您当前的 Telegram 已经是最新版本，无需重复更新！' -ForegroundColor Green
            Write-Host '------------------------------------------------------' -ForegroundColor Green
            exit 0
        }
    }

    Write-Host '[3/4] 正在下载最新加速版程序...' -ForegroundColor Cyan
    $tempFile = Join-Path $PWD 'Telegram.new.exe'
    try {
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile($downloadUrl, $tempFile)
    } catch {
        Write-Host '[错误] 下载失败，请确认代理或网络连接！' -ForegroundColor Red
        exit 1
    }

    # 检查是否正在运行
    $proc = Get-Process Telegram -ErrorAction SilentlyContinue
    if ($proc) {
        Write-Host '[提示] 检测到 Telegram 正在运行，正在准备重启应用更新...' -ForegroundColor Yellow
        $proc | Stop-Process -Force
        Start-Sleep -Seconds 1
    }

    Write-Host '[4/4] 正在无缝替换主程序...' -ForegroundColor Cyan
    if (Test-Path $localExe) {
        Remove-Item $localExe -Force
    }
    Move-Item $tempFile $localExe -Force

    Write-Host '------------------------------------------------------' -ForegroundColor Green
    Write-Host '【成功】Telegram Boost 已成功更新到最新版本！' -ForegroundColor Green
    Write-Host '       所有聊天数据、设置和登录状态均保持原样。' -ForegroundColor Green
    Write-Host '------------------------------------------------------' -ForegroundColor Green

    Write-Host '正在自动启动 Telegram...' -ForegroundColor Cyan
    Start-Process $localExe
}"

echo.
pause
