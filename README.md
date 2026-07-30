# ClickFlow

ClickFlow 0.3.0 是一个基于 `Qt 6 Widgets + C++20` 的桌面输入自动化工具，
支持 Windows 10/11 x64 和 macOS。

项目版本只在顶层 `CMakeLists.txt` 的 `project(ClickFlow VERSION ...)` 中
维护，并自动传递到应用与平台文件信息。为兼容已有用户配置，内部
QSettings 应用键继续使用 `QtClicker`。

## 功能

- 开始/停止连点
- 左键/右键
- 跟随当前鼠标或固定坐标
- 有限次数或无限次数
- 抖动半径
- 开始前倒计时
- 多配置预设
- 全局热键
- 窗口置顶
- Windows 全局键盘与鼠标操作录制
- Windows 目标窗口绑定录制与相对坐标回放
- 宏速度、有限/无限循环和轮次间隔
- 本机宏保存、重命名与删除

## Windows 键鼠录制

左侧进入“键鼠录制”页面。页面顶部始终显示当前控制热键，默认值为：

- `F9`：开始或停止录制；
- `F10`：开始或停止回放；
- `F8`：紧急停止当前连点、录制或回放。

热键可在“热键”页面修改。ClickFlow 会同时校验连点、坐标捕获、紧急停止、
宏录制和宏回放五组热键；任意重复、格式无效或被其他应用占用时，整组注册失败并
显示原因。

录制支持两种范围：

- “整个系统”记录跨应用操作，鼠标位置使用虚拟桌面坐标；
- “绑定一个窗口”可从窗口列表选择，也可拖动准星选窗。鼠标位置使用目标窗口
  客户区相对坐标，因此窗口移动后仍可回放。

绑定窗口回放要求客户区尺寸与录制时接近，允许的宽高差分别为 8 像素与 2% 中
较大的值。超出容差、窗口关闭、失焦或无法激活时会停止，不会继续操作其他窗口。
全局回放发现显示器布局无法容纳原坐标时也会拒绝启动。

鼠标轨迹会以约 16 毫秒间隔采样并自动合并近似直线移动；拖拽期间采用更保守的
压缩策略。回放支持 `0.5×`、`1×`、`1.5×`、`2×`，以及有限次数、无限循环和
每轮等待时间。连点、宏录制和宏回放严格互斥。

宏以版本化 JSON 文件保存在当前电脑的 ClickFlow 应用数据目录中，使用 UUID
作为文件名。数据不会上传、导出或写入日志。录制文件包含按键码和操作时间，可能
反映敏感输入；首次录制会提示风险，请勿在录制过程中输入密码、验证码或其他机密
信息。

普通权限的 ClickFlow 不能向管理员权限窗口注入输入，Windows UAC 安全桌面、
受保护应用及部分反作弊游戏也可能阻止钩子或模拟输入。程序不会自动请求提权。

## Windows 构建

推荐环境：

- Qt 6.8.3 `win64_msvc2022_64`
- Visual Studio 2022 或兼容 MSVC 工具集
- CMake 3.24+

不要把个人 Qt 路径写进仓库的 `CMakePresets.json`。在项目根目录创建
不会被 Git 跟踪的 `CMakeUserPresets.json`：

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

如果使用 Visual Studio 2026，把 `inherits` 改为
`windows-vs2026-debug`。配置、构建和测试：

```powershell
cmake --preset windows-local
cmake --build build/windows-msvc-debug --config Debug --parallel
ctest --test-dir build/windows-msvc-debug -C Debug --output-on-failure
```

Visual Studio 2026 对应的构建目录为 `build/windows-vs2026-debug`。

也可以不创建用户 preset：

```powershell
cmake -S . -B build/windows-vs2026-debug `
  -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64
cmake --build build/windows-vs2026-debug --config Debug --parallel
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

### Windows 打包

先构建 Release，再运行仓库中的部署脚本：

```powershell
cmake --build build/windows-vs2026-debug --config Release --parallel
.\scripts\package-windows.ps1 `
  -BuildDir .\build\windows-vs2026-debug `
  -OutputDir .\dist\QtClicker `
  -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin
```

脚本会复制应用并运行 `windeployqt`，产物无需把 Qt 永久加入系统
`PATH`。若旧版 `windeployqt` 无法识别较新的 Visual Studio，脚本会通过
Visual Studio Installer 定位并补齐 x64 VC Runtime DLL。

## macOS 构建

```bash
cmake --preset macos-debug
cmake --build build
ctest --test-dir build --output-on-failure
```

也可以直接手动指定：

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qtbase/lib/cmake
```

如果是 Intel Mac，可把 `CMAKE_PREFIX_PATH` 改成 `/usr/local/opt/qtbase/lib/cmake`。

## 平台说明

macOS 需要为应用授予“辅助功能”权限，否则无法发出全局点击事件。程序会在界面中提示这一点。

Windows 使用 `SendInput` 和 `RegisterHotKey`，不需要管理员权限。若某个
全局热键已被其他程序占用，应用会拒绝该组配置并显示错误。
