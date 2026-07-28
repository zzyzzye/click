# QtClicker

一个基于 `Qt 6 Widgets + C++20` 的跨平台桌面连点器，支持 Windows
10/11 x64 和 macOS。

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
