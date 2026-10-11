# ClickFlow

[![Windows 安装包](https://github.com/zzyzzye/click/actions/workflows/windows-package.yml/badge.svg)](https://github.com/zzyzzye/click/actions/workflows/windows-package.yml)

ClickFlow 是一个基于 Qt 6 Widgets 和 C++20 的桌面输入自动化工具，提供自动连点、全局热键，以及 Windows 全局键盘与鼠标操作的录制和回放。

- 下载稳定版本：[GitHub Releases](https://github.com/zzyzzye/click/releases)
- 查看 Windows 构建：[Windows 安装包工作流](https://github.com/zzyzzye/click/actions/workflows/windows-package.yml)

## 主要功能

- 可配置左键或右键自动连点；
- 支持跟随鼠标、固定坐标和随机抖动半径；
- 支持有限次数、无限循环和启动倒计时；
- 支持保存、加载、重命名和删除连点配置；
- 支持全局热键和窗口置顶；
- Windows 支持录制全局键盘、鼠标移动、按键、滚轮和拖拽；
- Windows 支持绑定目标窗口并按客户区相对坐标回放；
- 宏回放支持速度调整、有限或无限循环，以及轮次间隔；
- 步骤编辑器支持多点点击、按键、文字、等待，以及复制、排序、撤销重做；
- 步骤流程支持暂停继续、单轮试运行、步骤与轮次进度；
- 连点、录制、回放和步骤流程共享执行权，暂停时也不能启动其他任务。

## 平台支持

| 能力 | Windows x64 / ARM64 | macOS ARM64（M 系列） | Linux x64 / ARM64（X11） |
|---|---:|---:|---:|
| 自动连点 | ✅ | ✅ | ✅ |
| 全局热键 | ✅ | ✅ | ✅ |
| 配置预设 | ✅ | ✅ | ✅ |
| 键鼠宏录制与回放 | ✅ | — | — |
| 目标窗口绑定 | ✅ | — | — |
| 步骤编排、点击与等待 | 已实现 | 已实现 | 已实现（X11） |
| 手工按键步骤 | 常用单键和组合键 | 常用单键和组合键 | 单键 |
| 文字步骤 | Unicode | Unicode | 暂不支持 |
| 安装包格式 | EXE | DMG | DEB |

提供五种 64 位构建目标，不提供 32 位版本。Windows x64 面向 Windows 10/11，ARM64 面向 Windows 11 on Arm；macOS 面向 M 系列芯片；Linux DEB 基于 Ubuntu 24.04，依赖系统 Qt，桌面全局输入需要 X11/Xorg 会话，不支持 Wayland 全局输入。宏录制、回放与目标窗口绑定目前仍为 Windows 专属。

新步骤能力已通过假后端自动化验证与 macOS 本地构建；Windows/Linux 真实桌面输入、不同键盘布局、权限与多显示器仍需目标系统验证。“已实现”不表示所有目标应用都已实测。

## 编排步骤与多点点击

1. 进入“编排步骤”，填写流程名称，点击“添加步骤”。
2. 点击步骤可填写屏幕坐标、左/右键、次数和执行后等待时间。点击“3 秒后捕获当前位置”后，将鼠标移到目标；可用底部“停止”取消捕获。多点操作重复添加点击步骤即可。
3. 插入按键、文字与等待，使用复制、删除、上移、下移、撤销重做调整流程。文字以明文保存，请勿填写机密内容。
4. 设置整体轮数或无限循环及每轮等待。一轮从头执行完整步骤列表；点击步骤内的连续点击间隔为 10 毫秒。
5. 先“试运行一次”，确认正确后“保存流程”。从本机流程列表选择并点击“打开”即可重新编辑。

开始后锁定编辑，显示当前步骤、轮次和完成轮数；无限循环不显示百分比。“暂停”保留剩余等待，“继续”重新检查权限、屏幕布局及录制目标窗口；停止后再次开始从头运行。切换页面保留内容，新建、打开其他流程或关闭窗口前询问保存、放弃或取消；关闭取消后保留内容，但不自动恢复输入。

点击使用系统屏幕坐标，支持屏幕内的负坐标；布局变化或坐标位于屏幕间空隙时拒绝运行。检查全部位置后点击“确认当前屏幕布局”，再试运行。缩放、目标界面或窗口位置变化后也应重新捕获点位。

Windows 录制页可点击“在步骤编辑器打开选中录制”。未保存草稿转移到编辑器；已保存旧宏保持原样，流程另存到本机应用数据目录下的 `flows`。原始录制作为完整事件块保留移动、滚轮、拖拽和按下/释放，保留回放速度；可移动、复制或删除整个块，首期不编辑块内单个事件。暂停等待当前录制块结束，紧急停止始终有效。绑定窗口的录制块首期只可搭配等待步骤，避免混用屏幕与客户区坐标。

默认 `F11` 暂停/继续步骤流程；`F10` 在编排页开始流程，运行时停止流程；`F7` 在编排页立即捕获所选点击步骤坐标；`F8` 紧急停止。热键需手动启用，暂停热键也参与冲突检查。不支持的步骤在执行任何输入前提示，并保留编辑内容。

## 下载安装

Windows 用户可从 [Releases](https://github.com/zzyzzye/click/releases) 下载以下文件：

```text
ClickFlow-<version>-win64-setup.exe
ClickFlow-<version>-win64-setup.exe.sha256
ClickFlow-<version>-win-arm64-setup.exe
ClickFlow-<version>-macos-arm64.dmg
ClickFlow-<version>-linux-x64.deb
ClickFlow-<version>-linux-arm64.deb
```

Windows 安装器为所有用户安装到 `Program Files\ClickFlow`，并在安装和卸载时请求 UAC。开始菜单和桌面快捷方式均为可选项；安装后可从“设置 → 应用”或控制面板“程序和功能”卸载。macOS 打开 DMG 后将应用拖入 Applications；该包只有临时签名，未经开发者签名与 Apple 公证。Linux 可用 `sudo apt install ./ClickFlow-<version>-linux-<arch>.deb` 安装并解析依赖。

当前安装包未进行代码签名，Windows SmartScreen 可能显示“未知发布者”。建议在安装前核对 Release 附带的 SHA-256 文件。

### Windows 应用内覆盖更新

在“预设与关于”点击“检查更新”，发现正式发布的新版本后点击“下载更新”。应用显示下载百分比和已下载大小，支持取消与失败后重试。下载及 SHA-256 校验完成后，点击“重启并更新”：先处理未保存的录制/流程，再退出主程序、请求管理员授权，显示覆盖安装进度，完成后重新打开 ClickFlow。

更新沿用固定 AppId 和原安装目录，保留原来的快捷方式选择、配置、录制与流程，不先卸载、不创建另一套应用，也不重启电脑。开发构建或未注册的便携副本不能执行此覆盖更新。只有正式发布、版本号更高且安装包与校验文件齐全的 Release 才能更新，草稿不会被发现。

下载或校验失败时原版本保持不变；取消未保存确认或更新助手启动失败时不退出软件。取消管理员授权后由助手重新打开原版本；安装过程失败时会提示并尝试启动当前可用版本，不承诺安装失败自动回滚。macOS/Linux 仍跳转 Release 手动下载。Windows 的 UAC、原位覆盖和自动重开需在对应系统实测。

应用名称、组织名称和安装包发布者统一为 `ClickFlow`。设置与宏使用新的 `ClickFlow` 数据路径；旧版 `OpenAI/QtClicker` 路径中的数据不会自动迁移，升级前请备份原有配置与宏。

## Windows 键鼠宏

默认控制热键：

| 热键 | 操作 |
|---|---|
| `F9` | 开始或停止录制 |
| `F10` | 开始或停止回放 |
| `F11` | 暂停或继续步骤流程 |
| `F8` | 紧急停止连点、录制、回放或步骤流程 |

所有热键都可在“热键”页面修改。ClickFlow 会统一校验连点、坐标捕获、紧急停止、宏录制、宏回放和流程暂停热键；重复、无效或已被其他程序占用时，不会启用冲突配置。

第一次使用时可直接通过页面按钮操作；全局热键需要在“热键”页面手动启用。录制页的操作按钮固定在底部，不随内容滚动。

录制结束后生成仅保留在本次会话的草稿，可以先点击“试运行一次”，再点击“保存录制”命名保存。试运行会按当前速度操作真实桌面，固定执行一轮；草稿通过回放热键启动时也只执行一轮。取消命名或保存失败不会丢弃草稿，切换页面也会保留；重新录制或关闭窗口前会询问保存、放弃或取消。程序异常退出不能恢复内存草稿。

试运行与普通回放使用当前设置的快照，不会改写已有录制文件；保存新录制时写入当前回放设置。“连点设置已自动保存”仅指连点配置，不代表录制草稿已保存。

录制范围包括：

- **整个系统**：记录跨应用操作，鼠标位置使用虚拟桌面坐标；
- **绑定一个窗口**：通过窗口列表或拖动准星选择目标，使用客户区相对坐标，窗口移动后仍可回放。

目标窗口回放会校验窗口是否存在、能否激活以及客户区尺寸是否仍与录制时接近。窗口丢失、失焦、尺寸变化过大或全局坐标超出当前显示器布局时，回放会停止，不会继续向其他窗口注入输入。

鼠标轨迹以约 16 毫秒间隔采样，并自动合并近似直线移动。拖拽使用更保守的压缩策略。回放速度可设置为 `0.5×`、`1×`、`1.5×` 或 `2×`。

## 安全与隐私

- 宏以版本化 JSON 文件保存在本机 Qt 应用数据目录中，不会自动上传；
- 录制内容包含按键码和时间信息，可能反映密码、验证码或其他敏感输入；
- 请勿在录制期间输入机密信息；
- 普通权限进程不能向管理员权限窗口注入输入；
- Windows UAC 安全桌面、受保护应用和部分反作弊程序可能阻止钩子或模拟输入；
- 卸载器会删除执行卸载的 Windows 用户所保存的 ClickFlow 宏和配置，但不会跨账户修改其他用户的私有数据。

## 从源码构建

通用要求：

- CMake 3.24 或更高版本；
- 支持 C++20 的编译器；
- Windows/macOS 打包使用 Qt 6.8.3；Linux 使用 Ubuntu 24.04 的系统 Qt 6.4，包含 `Widgets`、`Network` 和 `Test` 组件。

项目版本由顶层 `CMakeLists.txt` 中的 `project(ClickFlow VERSION ...)` 提供，并自动写入应用版本、Windows 文件资源和安装包名称。

### Windows

推荐使用 Visual Studio 2022 x64 工具链和 Qt `win64_msvc2022_64`。个人 Qt 路径应写入不受 Git 跟踪的 `CMakeUserPresets.json`：

```json
{
  "version": 6,
  "include": ["CMakePresets.json"],
  "configurePresets": [
    {
      "name": "windows-local",
      "inherits": "windows-msvc-debug",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "D:/Qt/6.8.3/msvc2022_64"
      }
    }
  ]
}
```

配置、构建和测试：

```powershell
cmake --preset windows-local
cmake --build build/windows-msvc-debug --config Debug --parallel
ctest --test-dir build/windows-msvc-debug -C Debug --output-on-failure
```

也可以直接指定生成器和 Qt 路径：

```powershell
cmake -S . -B build/windows-msvc-debug `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64
cmake --build build/windows-msvc-debug --config Debug --parallel
ctest --test-dir build/windows-msvc-debug -C Debug --output-on-failure
```

### macOS

安装 Qt 后使用仓库预设：

```bash
cmake --preset macos-debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Homebrew 的常见 Qt 路径为 Apple Silicon 上的 `/opt/homebrew/opt/qtbase/lib/cmake`，或 Intel Mac 上的 `/usr/local/opt/qtbase/lib/cmake`。

## Windows 打包

### 便携目录

先构建 Release，再运行部署脚本：

```powershell
cmake --build build/windows-msvc-debug --config Release --parallel
.\scripts\package-windows.ps1 `
  -BuildDir .\build\windows-msvc-debug `
  -OutputDir .\dist\ClickFlow `
  -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin `
  -Configuration Release
```

脚本会复制 `ClickFlow.exe`，运行 `windeployqt`，并补齐 Qt 插件与对应架构 VC++ Runtime。ARM64 使用 `-Architecture arm64` 参数和 ARM64 Qt 路径。便携目录不会注册控制面板卸载入口。

### 安装包

仓库的 [Windows 安装包工作流](https://github.com/zzyzzye/click/actions/workflows/windows-package.yml) 使用 GitHub 托管的 Windows Runner 完成以下工作：

1. 分别准备 Qt 6.8.3 MSVC x64 和 ARM64；
2. 构建 Release；
3. 执行完整 CTest 测试集；
4. 部署 Qt 和 VC++ 运行库；
5. 使用 Inno Setup 编译安装器；
6. 生成 SHA-256 并分别上传 `ClickFlow-Windows-x64` 和 `ClickFlow-Windows-arm64` artifact。

该工作流仅支持手动触发，不会创建标签或自动发布 Release。正式发布时应从 `v<version>` 标签触发，确保安装包对应不可变的源码版本。

### macOS 与 Linux GitHub 打包

在 Actions 页面手动运行“macOS 安装包”或“Linux 安装包”，选择要构建的分支或标签。成功后进入 Summary → Artifacts 下载；这些工作流同样不会自动发布 Release。

也可以只运行“全平台安装包”，一次触发三个平台的五种架构任务，并在同一个 Summary 页面下载五份产物。

- macOS：`macos-15` ARM64 Runner，运行 `bash scripts/build-macos-release.sh`，面向 macOS 14 或更新版本的 M 系列芯片，产物为 `ClickFlow-macOS-arm64`，含 DMG 和 SHA-256。
- Linux：`ubuntu-24.04` 与 `ubuntu-24.04-arm`，运行 `bash scripts/build-linux-release.sh`，产物为 `ClickFlow-Linux-x64` / `ClickFlow-Linux-arm64`，含 DEB 和 SHA-256。
- 图形测试使用 `QT_QPA_PLATFORM=offscreen`，Linux 输入测试使用 Xvfb，不在真实桌面模拟输入。

Linux 本地构建前安装 `cmake ninja-build g++ qt6-base-dev qt6-svg-dev qt6-qpa-plugins libx11-dev libxtst-dev libxrandr-dev xvfb xauth dpkg-dev file`。macOS 打包需要 Ninja、Qt 和 `macdeployqt`。

## 维护者发布流程

本项目约定：你决定发版时，告知助手“准备发布”及目标版本号（可选）。助手负责核对实际变更、准备版本号和 `docs/releases/v<version>.md` 中文说明、完成相关检查，并提供本次可直接执行的 Git 命令；Git 提交、推送和版本标签由你手动执行。详细协作规则见 `AGENTS.md` 的“发版准备约定”。

1. 在 `CMakeLists.txt` 更新 `project(ClickFlow VERSION ...)`，并运行完整测试；
2. 提交版本变更并推送目标分支；
3. 创建并推送带注释的版本标签；
4. 推送标签自动触发“版本发布草稿”，核对版本和说明后打包五种平台与架构；
5. 全部构建成功后，自动核对 SHA-256，创建草稿 Release 并上传五个安装包及校验文件；
6. 在 GitHub Releases 检查中文说明和附件，并在对应系统验证安装、升级和卸载；
7. 检查通过后点击 Publish release 正式发布。

推送标签前，必须在 `docs/releases/v<version>.md` 写好该版本的中文发布说明。标题自动使用 `ClickFlow v<version>`，正文直接读取该文件。缺少说明或标签与项目版本不一致时，会在打包前停止。手动运行打包工作流仍只生成 Artifacts。

重跑失败任务会复用已经成功的打包任务；已有草稿会更新说明和同名附件。自动流程拒绝覆盖已经正式发布的 Release。若重新运行全部任务，则五种安装包都会重新构建。

标签示例：

```powershell
$version = '<version>'
git tag -a "v$version" -m "发布：v$version"
git push origin "v$version"
```

发布前必须保证以下版本一致：

- `CMakeLists.txt` 中的项目版本；
- Git 标签 `v<version>`；
- Actions 选择的构建来源；
- 安装包文件名和控制面板显示版本；
- GitHub Release 使用的标签。

## 平台限制

- Windows 应用本身默认以普通权限运行，不能操作提权窗口或 UAC 安全桌面；
- Windows 全局热键依赖 `RegisterHotKey`，已被其他程序占用的组合无法注册；
- Windows 输入回放依赖 `SendInput`，目标程序可能主动拒绝模拟输入；
- macOS 需要授予“辅助功能”权限才能发出全局点击事件；
- macOS 与 Linux 当前不支持键鼠宏录制、目标窗口绑定；Linux 全局输入仅支持 X11。
