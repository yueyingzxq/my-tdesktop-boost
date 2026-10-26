@echo off
chcp 65001 >nul
title Telegram Boost 一键自动更新器

:: =====================================================================
::                    【Telegram Boost 仓库配置】
:: 您的专属 GitHub 仓库名。更新脚本会直接调用此仓库的 Releases API 检查更新。
:: =====================================================================
set "GITHUB_REPO=yueyingzxq/my-tdesktop-boost"
set "RELEASE_TAG=latest-boost"
:: =====================================================================

echo ======================================================
echo           Telegram Boost 一键自动更新工具
echo ======================================================
echo  目标仓库: %GITHUB_REPO%
echo  发布标签: %RELEASE_TAG%
echo ======================================================
echo.

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

powershell -NoProfile -ExecutionPolicy Bypass -Command "& {
    $repo = '%GITHUB_REPO%'
    $tag = '%RELEASE_TAG%'
    $apiUrl = \"https://api.github.com/repos/$repo/releases/tags/$tag\"
    
    # 启用 TLS 1.2 并自动继承系统代理（v2rayN）
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    [System.Net.WebRequest]::DefaultWebProxy = [System.Net.WebRequest]::GetSystemWebProxy()
    [System.Net.WebRequest]::DefaultWebProxy.Credentials = [System.Net.CredentialCache]::DefaultCredentials

    Write-Host '[1/4] 正在连接 GitHub API 检查更新...' -ForegroundColor Cyan
    try {
        $release = Invoke-RestMethod -Uri $apiUrl -Headers @{'User-Agent'='Telegram-Updater'} -TimeoutSec 15
    } catch {
        # 如果未找到 latest-boost 标签，回退尝试查询最新 release
        try {
            $apiUrlLatest = \"https://api.github.com/repos/$repo/releases/latest\"
            $release = Invoke-RestMethod -Uri $apiUrlLatest -Headers @{'User-Agent'='Telegram-Updater'} -TimeoutSec 15
        } catch {
            Write-Host '[提示] 暂未获取到 Release 发布记录（云端 Actions 正在打包上传中，请稍候几分钟再试）' -ForegroundColor Yellow
            Write-Host \"GitHub API 响应: $($_.Exception.Message)\" -ForegroundColor Gray
            exit 0
        }
    }

    # 优先寻找 Telegram.exe，其次寻找包含 Telegram 的 zip 压缩包
    $assetExe = $release.assets | Where-Object { $_.name -eq 'Telegram.exe' } | Select-Object -First 1
    $assetZip = $release.assets | Where-Object { $_.name -like '*Telegram*.zip' } | Select-Object -First 1
    
    $targetAsset = if ($assetExe) { $assetExe } else { $assetZip }

    if (-not $targetAsset) {
        Write-Host '[提示] 云端 Release 暂未包含可执行文件或压缩包，请稍候再试！' -ForegroundColor Yellow
        exit 0
    }

    $downloadUrl = $targetAsset.browser_download_url
    $remoteSize = $targetAsset.size
    $remoteDate = [DateTime]::Parse($targetAsset.updated_at)
    $isZip = $targetAsset.name.EndsWith('.zip')

    Write-Host \"[2/4] 获取到云端最新构建：$($release.name)\" -ForegroundColor Green
    Write-Host \"      资源名称：$($targetAsset.name)\" -ForegroundColor Gray
    Write-Host \"      发布时间：$($remoteDate.ToLocalTime())\" -ForegroundColor Gray
    Write-Host \"      文件大小：$([Math]::Round($remoteSize / 1MB, 2)) MB\" -ForegroundColor Gray

    # 检查本地是否有 Telegram.exe
    $localExe = Join-Path $PWD 'Telegram.exe'
    if (Test-Path $localExe) {
        $localSize = (Get-Item $localExe).Length
        # 如果直接是 exe 且文件大小相同，判定为最新版
        if (-not $isZip -and ($localSize -eq $remoteSize)) {
            Write-Host '------------------------------------------------------' -ForegroundColor Green
            Write-Host '【恭喜】您当前的 Telegram 已经是最新版本，无需更新！' -ForegroundColor Green
            Write-Host '------------------------------------------------------' -ForegroundColor Green
            exit 0
        }
    }

    Write-Host \"[3/4] 正在下载最新加速版程序 ($($targetAsset.name))...\" -ForegroundColor Cyan
    $downloadPath = if ($isZip) { Join-Path $PWD 'Telegram_update.zip' } else { Join-Path $PWD 'Telegram.new.exe' }

    try {
        $webClient = New-Object System.Net.WebClient
        $webClient.Proxy = [System.Net.WebRequest]::DefaultWebProxy
        $webClient.DownloadFile($downloadUrl, $downloadPath)
    } catch {
        Write-Host '[错误] 下载失败，请检查网络或 v2rayN 系统代理状态！' -ForegroundColor Red
        Write-Host $_.Exception.Message -ForegroundColor Yellow
        exit 1
    }

    # 检查 Telegram 是否正在运行
    $proc = Get-Process Telegram -ErrorAction SilentlyContinue
    if ($proc) {
        Write-Host '[提示] 检测到 Telegram 正在运行，正在准备重启应用更新...' -ForegroundColor Yellow
        $proc | Stop-Process -Force
        Start-Sleep -Seconds 1
    }

    Write-Host '[4/4] 正在解压并无缝替换主程序...' -ForegroundColor Cyan
    if ($isZip) {
        # 解压 zip 中的 Telegram.exe
        $tempExtractDir = Join-Path $PWD '_tg_update_temp'
        if (Test-Path $tempExtractDir) { Remove-Item $tempExtractDir -Recurse -Force }
        Expand-Archive -Path $downloadPath -DestinationPath $tempExtractDir -Force
        
        $newExe = Join-Path $tempExtractDir 'Telegram.exe'
        if (Test-Path $newExe) {
            Move-Item $newExe $localExe -Force
        }
        $newUpdater = Join-Path $tempExtractDir 'Updater.exe'
        if (Test-Path $newUpdater) {
            Move-Item $newUpdater (Join-Path $PWD 'Updater.exe') -Force
        }
        Remove-Item $tempExtractDir -Recurse -Force
        Remove-Item $downloadPath -Force
    } else {
        Move-Item $downloadPath $localExe -Force
    }

    Write-Host '------------------------------------------------------' -ForegroundColor Green
    Write-Host '【成功】Telegram Boost 已成功更新为最新版本！' -ForegroundColor Green
    Write-Host '       所有聊天记录、登录凭据和个人设置 100% 完整保留。' -ForegroundColor Green
    Write-Host '------------------------------------------------------' -ForegroundColor Green

    Write-Host '正在自动启动 Telegram...' -ForegroundColor Cyan
    Start-Process $localExe
}"

echo.
pause
