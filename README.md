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
- 连点、宏录制和宏回放共享统一状态协调，不能同时运行。

## 平台支持

| 能力 | Windows 10/11 x64 | macOS |
|---|---:|---:|
| 自动连点 | ✅ | ✅ |
| 全局热键 | ✅ | ✅ |
| 配置预设 | ✅ | ✅ |
| 键鼠宏录制与回放 | ✅ | — |
| 目标窗口绑定 | ✅ | — |
| 图形化安装包 | ✅ | — |

Windows 是当前主要发布平台。macOS 可以从源码构建，但尚未提供键鼠宏和正式安装包。

## 下载安装

Windows 用户可从 [Releases](https://github.com/zzyzzye/click/releases) 下载以下文件：

```text
ClickFlow-<version>-win64-setup.exe
ClickFlow-<version>-win64-setup.exe.sha256
```

安装器面向 Windows 10/11 x64，为所有用户安装到 `Program Files\ClickFlow`，并在安装和卸载时请求 UAC。开始菜单和桌面快捷方式均为可选项；安装后可从“设置 → 应用”或控制面板“程序和功能”卸载。

当前安装包未进行代码签名，Windows SmartScreen 可能显示“未知发布者”。建议在安装前核对 Release 附带的 SHA-256 文件。

## Windows 键鼠宏

默认控制热键：

| 热键 | 操作 |
|---|---|
| `F9` | 开始或停止录制 |
| `F10` | 开始或停止回放 |
| `F8` | 紧急停止连点、录制或回放 |

所有热键都可在“热键”页面修改。ClickFlow 会统一校验连点、坐标捕获、紧急停止、宏录制和宏回放热键；重复、无效或已被其他程序占用时，不会启用冲突配置。

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
- Qt 6.8.3，包含 `Widgets` 和 `Test` 组件。

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

脚本会复制 `ClickFlow.exe`，运行 `windeployqt`，并补齐 Qt 插件与 x64 VC++ Runtime。便携目录不会注册控制面板卸载入口。

### 安装包

仓库的 [Windows 安装包工作流](https://github.com/zzyzzye/click/actions/workflows/windows-package.yml) 使用 GitHub 托管的 Windows Runner 完成以下工作：

1. 准备 Qt 6.8.3 MSVC x64；
2. 构建 Release；
3. 执行完整 CTest 测试集；
4. 部署 Qt 和 VC++ 运行库；
5. 使用 Inno Setup 编译安装器；
6. 生成 SHA-256 并上传 `ClickFlow-Windows-x64` artifact。

该工作流仅支持手动触发，不会创建标签或自动发布 Release。正式发布时应从 `v<version>` 标签触发，确保安装包对应不可变的源码版本。

## 维护者发布流程

1. 在 `CMakeLists.txt` 更新 `project(ClickFlow VERSION ...)`，并运行完整测试；
2. 提交版本变更并推送目标分支；
3. 创建并推送带注释的版本标签；
4. 在 Actions 页面手动运行“Windows 安装包”，构建来源选择该标签；
5. 下载 `ClickFlow-Windows-x64` artifact，并核对安装包版本和 SHA-256；
6. 基于同一标签创建 GitHub Release，上传 EXE 和 `.sha256`；
7. 在允许安装软件的 Windows 电脑验证安装、覆盖升级和卸载。

标签示例：

```powershell
$version = '<version>'
git tag -a "v$version" -m "release: 发布v$version"
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
- macOS 当前不支持键鼠宏录制、目标窗口绑定或官方安装包。
