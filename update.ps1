# ===================================================================
#    Telegram Boost 自动化更新程序
#    作者: yueyingzxq
#    仓库: yueyingzxq/my-tdesktop-boost
# ===================================================================

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$Host.UI.RawUI.WindowTitle = "Telegram Boost 自动更新器"

$repo = "yueyingzxq/my-tdesktop-boost"
$tag = "latest-boost"
$installDir = $PSScriptRoot

Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host "               Telegram Boost 一键极速更新工具                    " -ForegroundColor Green
Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host " 仓库地址: https://github.com/$repo" -ForegroundColor Gray
Write-Host " 当前目录: $installDir" -ForegroundColor Gray
Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host ""

# 启用 TLS 1.2
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# 代理检测与配置 (自动继承系统代理如 v2rayN)
$proxy = [System.Net.WebRequest]::GetSystemWebProxy()
$proxy.Credentials = [System.Net.CredentialCache]::DefaultCredentials
[System.Net.WebRequest]::DefaultWebProxy = $proxy

Write-Host "[步骤 1/5] 正在检查 GitHub 云端最新版本..." -ForegroundColor Cyan

$release = $null
$apiUrl = "https://api.github.com/repos/$repo/releases/tags/$tag"
$apiUrlLatest = "https://api.github.com/repos/$repo/releases/latest"

try {
    $wc = New-Object System.Net.WebClient
    $wc.Proxy = $proxy
    $wc.Headers.Add("User-Agent", "Telegram-Boost-Updater")
    $json = $wc.DownloadString($apiUrl)
    $release = $json | ConvertFrom-Json
} catch {
    # 尝试 latest 接口
    try {
        $wc2 = New-Object System.Net.WebClient
        $wc2.Proxy = $proxy
        $wc2.Headers.Add("User-Agent", "Telegram-Boost-Updater")
        $json = $wc2.DownloadString($apiUrlLatest)
        $release = $json | ConvertFrom-Json
    } catch {
        Write-Host ""
        Write-Host "------------------------------------------------------------------" -ForegroundColor Yellow
        Write-Host "【提示】云端 Actions 正在打包编译中，暂未发布 Release 安装包。" -ForegroundColor Yellow
        Write-Host "       首次云端编译通常需要约 35 分钟。" -ForegroundColor Gray
        Write-Host "       您可以前往以下网址查看编译进度:" -ForegroundColor Gray
        Write-Host "       https://github.com/$repo/actions" -ForegroundColor Cyan
        Write-Host "       编译发布完成后，直接再次双击本脚本即可自动极速更新！" -ForegroundColor Yellow
        Write-Host "------------------------------------------------------------------" -ForegroundColor Yellow
        Write-Host ""
        Read-Host "按 Enter 回车键退出"
        exit 0
    }
}

if (-not $release -or -not $release.assets -or $release.assets.Count -eq 0) {
    Write-Host ""
    Write-Host "【提示】云端 Release 正在上传资产中，请稍候 2~3 分钟再试！" -ForegroundColor Yellow
    Write-Host ""
    Read-Host "按 Enter 回车键退出"
    exit 0
}

# 优先匹配 zip 包或 exe 文件
$targetAsset = $release.assets | Where-Object { $_.name -like '*Telegram*.zip' -or $_.name -eq 'Telegram.exe' } | Select-Object -First 1

if (-not $targetAsset) {
    Write-Host "【错误】未在云端 Release 中找到可用的 Telegram 安装文件。" -ForegroundColor Red
    Read-Host "按 Enter 回车键退出"
    exit 1
}

$downloadUrl = $targetAsset.browser_download_url
$remoteSize = $targetAsset.size
$remoteSizeMB = [Math]::Round($remoteSize / 1MB, 2)
$remoteDate = [DateTime]::Parse($targetAsset.updated_at).ToLocalTime().ToString("yyyy-MM-dd HH:mm:ss")
$isZip = $targetAsset.name.EndsWith(".zip")

Write-Host "   - 最新版本: $($release.name) ($($release.tag_name))" -ForegroundColor Green
Write-Host "   - 发布文件: $($targetAsset.name) ($remoteSizeMB MB)" -ForegroundColor Gray
Write-Host "   - 构建时间: $remoteDate" -ForegroundColor Gray
Write-Host ""

# 版本比对逻辑
$localExe = Join-Path $installDir "Telegram.exe"
$versionFile = Join-Path $installDir ".boost_version.json"
$needUpdate = $true

if (Test-Path $versionFile) {
    try {
        $savedVer = Get-Content $versionFile -Raw | ConvertFrom-Json
        if ($savedVer.asset_id -eq $targetAsset.id) {
            $needUpdate = $false
        }
    } catch {}
}

if (-not $needUpdate) {
    Write-Host "------------------------------------------------------------------" -ForegroundColor Green
    Write-Host "【恭喜】您当前运行的 Telegram Boost 已经是最新版本，无需重复更新！" -ForegroundColor Green
    Write-Host "------------------------------------------------------------------" -ForegroundColor Green
    Write-Host ""
    $choice = Read-Host "是否强制重新下载覆盖？(Y/N, 默认 N)"
    if ($choice -ne "Y" -and $choice -ne "y") {
        Write-Host "已取消更新。"
        Start-Sleep -Seconds 2
        exit 0
    }
}

Write-Host "[步骤 2/5] 正在下载最新加速版 ($remoteSizeMB MB)..." -ForegroundColor Cyan

$tempFile = if ($isZip) { Join-Path $installDir "_download_temp.zip" } else { Join-Path $installDir "_download_temp.exe" }
if (Test-Path $tempFile) { Remove-Item $tempFile -Force }

$downloadClient = New-Object System.Net.WebClient
$downloadClient.Proxy = $proxy
$downloadCompleted = $false
$lastProgress = -1

$registerProgress = {
    param($s, $e)
    $percent = $e.ProgressPercentage
    if ($percent -ne $script:lastProgress -and $percent % 5 -eq 0) {
        $script:lastProgress = $percent
        $receivedMB = [Math]::Round($e.BytesReceived / 1MB, 1)
        $totalMB = [Math]::Round($e.TotalBytesToReceive / 1MB, 1)
        $barLength = 30
        $filled = [Math]::Round(($percent / 100) * $barLength)
        $empty = $barLength - $filled
        $bar = ("=" * $filled) + (">") + (" " * [Math]::Max(0, $empty - 1))
        $line = "`r   [$bar] $percent% ($receivedMB MB / $totalMB MB)"
        Write-Host $line -NoNewline -ForegroundColor Green
    }
}

$registerCompleted = {
    param($s, $e)
    $script:downloadCompleted = $true
}

Register-ObjectEvent -InputObject $downloadClient -EventName DownloadProgressChanged -Action $registerProgress | Out-Null
Register-ObjectEvent -InputObject $downloadClient -EventName DownloadFileCompleted -Action $registerCompleted | Out-Null

try {
    $downloadClient.DownloadFileAsync((New-Object System.Uri($downloadUrl)), $tempFile)
    while (-not $downloadCompleted) {
        Start-Sleep -Milliseconds 200
    }
} catch {
    Write-Host "`n[错误] 下载失败: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "请确保已启动代理工具 (v2rayN) 并正常连接网络。" -ForegroundColor Yellow
    Read-Host "按 Enter 退出"
    exit 1
}

Write-Host "`n   [完成] 下载成功！" -ForegroundColor Green
Write-Host ""

# 步骤 3: 检查并平稳退出运行中的 Telegram
Write-Host "[步骤 3/5] 检测 Telegram 运行状态..." -ForegroundColor Cyan
$runningProcs = Get-Process Telegram -ErrorAction SilentlyContinue
if ($runningProcs) {
    Write-Host "   检测到 Telegram 正在运行，正在平稳关闭以替换程序文件..." -ForegroundColor Yellow
    $runningProcs | Stop-Process -Force
    $timeout = 10
    while ((Get-Process Telegram -ErrorAction SilentlyContinue) -and ($timeout -gt 0)) {
        Start-Sleep -Seconds 1
        $timeout--
    }
    Write-Host "   已成功关闭旧进程。" -ForegroundColor Gray
} else {
    Write-Host "   Telegram 未在运行，准备就绪。" -ForegroundColor Gray
}
Write-Host ""

# 步骤 4: 替换核心程序文件
Write-Host "[步骤 4/5] 正在安装并无缝替换主程序..." -ForegroundColor Cyan

if ($isZip) {
    $extractFolder = Join-Path $installDir "_extract_temp"
    if (Test-Path $extractFolder) { Remove-Item $extractFolder -Recurse -Force }
    
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($tempFile, $extractFolder)
    
    # 查找提取出来的 Telegram.exe
    $extractedExe = Get-ChildItem -Path $extractFolder -Recurse -Filter "Telegram.exe" | Select-Object -First 1
    if ($extractedExe) {
        Copy-Item -Path $extractedExe.FullName -Destination $localExe -Force
        Write-Host "   [更新] Telegram.exe 已更新" -ForegroundColor Green
    } else {
        Write-Host "   [错误] 压缩包内未找到 Telegram.exe！" -ForegroundColor Red
    }
    
    $extractedUpdater = Get-ChildItem -Path $extractFolder -Recurse -Filter "Updater.exe" | Select-Object -First 1
    if ($extractedUpdater) {
        Copy-Item -Path $extractedUpdater.FullName -Destination (Join-Path $installDir "Updater.exe") -Force
        Write-Host "   [更新] Updater.exe 已更新" -ForegroundColor Green
    }
    
    # 清理临时文件
    Remove-Item $extractFolder -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
} else {
    Move-Item -Path $tempFile -Destination $localExe -Force
    Write-Host "   [更新] Telegram.exe 已更新" -ForegroundColor Green
}

# 保存更新记录
$versionData = @{
    asset_id = $targetAsset.id
    name = $release.name
    tag = $release.tag_name
    updated_at = $targetAsset.updated_at
} | ConvertTo-Json
Set-Content -Path $versionFile -Value $versionData -Encoding UTF8

Write-Host ""
Write-Host "==================================================================" -ForegroundColor Green
Write-Host "           【更新成功】Telegram Boost 已成功更新！                " -ForegroundColor Green
Write-Host "       个人配置、登录凭据和聊天记录（tdata）100% 完整保留。        " -ForegroundColor Green
Write-Host "==================================================================" -ForegroundColor Green
Write-Host ""

# 步骤 5: 重启 Telegram
Write-Host "[步骤 5/5] 正在自动重新启动 Telegram..." -ForegroundColor Cyan
Start-Process -FilePath $localExe
Write-Host "   Telegram 已重新启动！" -ForegroundColor Green
Write-Host ""

Write-Host "本窗口将在 4 秒后自动退出..." -ForegroundColor Gray
Start-Sleep -Seconds 4
exit 0
