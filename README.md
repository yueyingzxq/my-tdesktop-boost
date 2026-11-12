# 🚀 Telegram Desktop Boost 极速增强版

<p align="center">
  <img src="Telegram/Resources/art/icon256.png" width="128" height="128" alt="Telegram Desktop Boost" />
</p>

<p align="center">
  <b>基于官方 Telegram Desktop 最新源码构建 · 专为提升国内网络体验与流媒体体验打造</b><br>
  突破单线程下载瓶颈 · 沉浸式流媒体连续播放 · 软件内无缝自动更新 · 100% 保留官方安全性与隐私
</p>

---

## ✨ 核心特性一览

| 功能特性 | 官方原版 Telegram | Telegram Boost 增强版 |
| :--- | :--- | :--- |
| **文件/视频下载** | 128KB小分片/易限速，经常只有几 MB/s | **可调两档体验：极速档 (1MB大分片+16通道) / 标准档 (128KB)**，对齐 Nekogram 吃满节点带宽 🚀 |
| **视频连续播放** | 播放完停止，需手动点击下一条 | **YouTube 风格自动连播开关**，看完自动切集 🎬 |
| **控制栏自动隐藏** | 自动切集后进度条常驻遮挡画面 | **切集 1.1 秒后自动优雅淡出隐藏**，免动鼠标 🖥️ |
| **全屏快捷键** | 仅视频支持，图片无法 F 全屏 | **图片与视频全支持 `F` 键秒级全屏**，偏好持久化 🔍 |
| **视频转发体验** | 转发跳出播放界面，打断观影 | **原位转发浮层**，转发时视频持续播放不卡顿 ↗️ |
| **软件更新方式** | 官方更新会覆盖定制功能 | **内置 GitHub 原生更新**，左下角一键静默热更新 🔄 |

---

## 📥 新手从零上手指南（2分钟搞定）

无论您是第一次使用 Telegram，还是从原版转过来的老玩家，跟着以下步骤即可轻松配置：

### 第一步：下载软件
前往 [**Releases 页面**](https://github.com/yueyingzxq/my-tdesktop-boost/releases/tag/latest-boost) 下载适合您操作系统的最新版本：

* **Windows 用户**：下载 `Telegram_x64_qt6.zip`。
  > 💡 **放置建议**：解压到一个独立的日常文件夹（推荐如 `D:\Software\Telegram\` 或 `E:\Telegram_Boost\`）。**请勿解压在 C 盘 Program Files 或桌面临时文件夹**，防止 Windows 权限拦截导致自动更新失败。
* **macOS 用户**：下载 `Telegram_macOS.zip`。
  > 💡 **安装建议**：解压后将 `Telegram.app` 拖入 `应用程序` (Applications) 文件夹。首次若提示“无法打开，因为无法验证开发者”，在“应用程序”中**右键 Telegram 选择“打开”**即可，或在终端执行 `xattr -cr /Applications/Telegram.app` 解除隔离。

### 第二步：老用户数据无缝迁移（免重新登录）
如果您以前用过官方 Telegram 或其他 Telegram 客户端：
1. 退出之前的 Telegram。
2. 打开旧版所在的文件夹，找到名为 **`tdata`** 的文件夹。
3. 把整个 `tdata` 文件夹**复制**到本增强版的文件夹中。
4. 双击打开 `Telegram.exe` —— **瞬间免密免验证码登录，所有联系人、聊天记录、置顶完全一致！**

### 第三步：界面汉化（可选）
官方默认无简体中文，可以在任意聊天框点击或在浏览器打开下方官方汉化包链接：
👉 [点击一键安装简体中文语言包](https://t.me/setlanguage/zhcncc)

### 第四步：核心功能与开关位置说明

#### 1. ⚡ Network Boost（多通道极速并发与流媒体秒播）
进入 **设置 (Settings) -> 高级 (Advanced) -> 数据与存储 (Data and Storage) -> Network Boost**：
* **🟢 开启（默认开启，推荐）**：
  - **大文件下载**：开启 1MB 超大分片 + 16 通道并发，拉满物理下载带宽；
  - **在线视频播放**：128KB 规范切片 + 8MB 激进预读缓冲 + 32 切片并发流水线，4K 原画秒播无延迟；
  - **长连接保活**：120 秒 Keep-Alive 热连接，切换视频与连续下载免去握手开销。
* **⚪ 关闭**：恢复官方保守的 128KB 小分片与标准通道数。
无需重启软件，开关切换立即生效！

#### 2. 🎬 自动连续播放与沉浸全屏
* 视频播放时，播放器控制条右侧新增 **YouTube 风格滑动开关**，点击即可随时开启/关闭自动连续播放；
* 开启后播完当前视频自动无缝衔接下一个，切换后 1 秒内播放进度条自动平滑淡出隐藏；
* 全局支持按 `F` 键快捷全屏（图片与视频均支持）；
* 手动 `Ctrl + 右键` 切换下一媒体时，图片保持全屏展示，切换至视频后进度条自动隐藏。

#### 3. ↗️ 原位转发（播放不中断）
* 观看视频时右键点击“转发”，视频在底层持续流畅播放，转发目标面板在上层浮动弹出，观看与转发两不耽误。

---

## 🔄 软件更新说明

本版本直接接入了 GitHub Releases 官方原生更新流：
1. **软件内一键无缝更新（首选）**：
   当仓库有新构建发布后，Telegram 左侧聊天列表最下方会自动亮起蓝色的 **【更新 Telegram】** 按钮（或在“设置 -> 高级 -> 检查更新”）。
   直接点击该按钮，软件自动在后台下载并毫秒级重启完成更新，**个人数据与登录状态 100% 完整保留**。
2. **备用一键更新脚本**：
   目录下附带 `一键更新.bat`，如遇特殊网络状况也可双击执行强力更新。

---

## 🛠️ 项目构建说明（开发者）

本仓库已配置 GitHub Actions 跨平台自动化流水线：
* **Windows 构建流水线** (`.github/workflows/win.yml`)：基于 MSVC & Qt 6 构建 Windows x64 极速便携版。
* **macOS 构建流水线** (`.github/workflows/mac.yml`)：基于 Apple Silicon / macOS 14 & Qt 6 构建 macOS 原生 App。

每次推送到 `feature/speed-boost` 或 `dev` 分支时，Actions 会自动触发两端并行全自动编译打包，并将产物同步发布至 [**Releases (latest-boost)**](https://github.com/yueyingzxq/my-tdesktop-boost/releases/tag/latest-boost)。
- **编译器**：MSVC 2022 / 2026 x64
- **Qt 版本**：Qt 6.11.2 (Static)
- **依赖库**：OpenSSL 3.5+, mozjpeg, zlib, liblcms2, tg_angle

---

## 📄 开源许可证

本项目基于 [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) 官方源码派生，遵循 **GPLv3 with OpenSSL Exception** 开源协议。
