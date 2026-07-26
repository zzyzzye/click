# QtClicker

一个基于 `Qt 6 Widgets + C++20` 的桌面连点器，当前版本先支持 macOS，并为后续 Windows 后端预留了结构。

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

## 构建

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

## 运行前说明

macOS 需要为应用授予“辅助功能”权限，否则无法发出全局点击事件。程序会在界面中提示这一点。
