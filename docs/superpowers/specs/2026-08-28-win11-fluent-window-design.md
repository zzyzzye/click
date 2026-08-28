# Win11 Fluent 窗口外壳（Mica 透出方案）设计

## 背景与目标

ClickFlow 是基于 Qt 6 Widgets 的桌面连点工具，现有 UI 采用卡片化布局 + 蓝色主色 + 统一控件尺寸，样式集中在 [src/app/UiStyle.cpp](../../../src/app/UiStyle.cpp) 的单一 QSS 字符串中。

本设计为 Windows 11 主窗口添加原生 Fluent 外壳效果：**圆角窗口 + Mica 云母背景 + 暗色标题栏适配**，使 Mica 从卡片间隙透出，呈现 Win11 设置应用般的质感。**保留全部现有控件与 QSS 结构**，只让窗口外壳和间隙背景变透明。

> 关键认知：DWM API 只动窗口外壳（标题栏、边框、背景云母效果），QSS 管的是控件外观。两者不同层级，QSS 删不掉；删除会使控件退化成 Qt 默认灰扑扑外观，根本不是 Fluent。

## 决策记录

| 决策 | 选择 | 理由 |
|---|---|---|
| 效果范围 | Mica 透出 | 圆角 + Mica + 暗色标题栏，QSS 仅改 2-3 处背景为透明，卡片/控件全保留 |
| 暗色模式 | 亮色优先 + 留接口 | 搭好主题检测与切换管路，内容 QSS 暂固定亮色，后续加暗色变体时直接复用 |

### 已知取舍

Win11 系统暗色模式下，标题栏 + Mica 会偏暗，而内容卡片仍为亮色，视觉不统一，直到未来补暗色 QSS 变体。这是「亮色优先」的预期结果。

## 平台分级

| 平台 | 行为 |
|---|---|
| Win11（build ≥ 22000） | 启用 Mica + 圆角 + `WA_TranslucentBackground` + 透明 QSS 变体 |
| Win10 | DWM 调用静默失败，保持现有不透明 `#f4f5f7`，无回归 |
| macOS / 测试 offscreen | no-op，行为不变 |

## 架构：新增 WindowStyleService 接口

仿照现有 `ClickBackend` / `HotkeyService` / `WindowService` 的平台接口 + 工厂模式，新增窗口样式平台服务。

### 接口定义

```cpp
// src/platform/WindowStyleService.h
class WindowStyleService {
 public:
  virtual ~WindowStyleService() = default;
  virtual void prepare(QWidget* window) = 0;    // show 前：按需开 WA_TranslucentBackground
  virtual void apply(QWidget* window) = 0;      // show 后：设 DWM 圆角/Mica/暗色标题栏
  virtual bool usesBackdrop() const = 0;        // 平台是否会真渲染 Mica（驱动 QSS 变体）
  virtual bool prefersDarkTheme() const = 0;    // 系统主题（未来暗色内容用）
};
std::unique_ptr<WindowStyleService> createWindowStyleService();
```

### 平台实现

- **Windows 实现** `WindowsWindowStyle`（新文件 `src/platform/windows/WindowsWindowStyle.cpp`）：调用 DWM + 读注册表系统主题。
- **macOS / 测试**：no-op 实现，`usesBackdrop()` 返回 false。
- macOS 的 no-op 在 `PlatformServicesMac.mm` 内提供；测试路径用默认 no-op。

### 注入方式

MainWindow 大构造函数尾部新增可选参数 `std::unique_ptr<WindowStyleService> = {}`：

- null 时用 no-op（测试路径不变）。
- 无参构造函数传 `createWindowStyleService()`。
- 现有 2 参测试构造函数签名**不变**，仅转发时补 `{}`。

## 关键机制

### Win11 检测

`platformName()=="windows" && build≥22000`（读注册表 `CurrentBuildNumber`）。单一布尔同时驱动 `WA_TranslucentBackground` 与 QSS 透明变体，保证两者一致，Win10/offscreen 都走不透明路径。

纯函数 `isWindows11OrLater(quint32 build)`：`build >= 22000`。

### DWM 属性应用时机

override `MainWindow::showEvent` 调 `apply(this)`。覆盖两类场景：

1. 首次 show；
2. `applyWindowOnTop` 切换置顶（`setWindowFlag` + `show()`）后，若重建原生窗口会丢 DWM 属性，showEvent 重新应用；若不重建也无害（幂等）。

### WA_TranslucentBackground

在构造期 `buildUi` 调用 `prepare(this)` 设置一次（早于首次 show）。Qt 在窗口重建时自动重应用该属性。

### 回退

DWM 调用失败返回非零，忽略即可。Win10 既不开 translucent 也不设 DWM，外观与现在一致。

## QSS 改动

`clickFlowStyleSheet(bool translucent = false)`，默认 false（不透明，测试/Win10/macOS 用）：

- **透明变体（translucent=true）**：`QMainWindow`、`#contentSurface`、`#contentPages`、`QScrollArea` 及页面根背景设 `transparent`；**卡片 `#settingsCard`、`#statusStrip`、`#actionBar`、侧栏 `#navigationSidebar` 保持不透明**（白/浅灰），Mica 从间隙透出。
- **不透明变体**：与现在完全一致。
- MainWindow 用 `clickFlowStyleSheet(styleService_->usesBackdrop())` 选择变体。

### 测试影响

现有测试断言（`QComboBox::down-arrow`、`#sidebarNavigation { background: transparent`、控件高度 40/44px）在两个变体下都成立，不受影响。`usesClickFlowControlChrome` 测试只断言 QSS 含特定子串，未断言主背景色 `#f4f5f7`。

## DWM 常量与系统主题

| DWM 属性 | 值 | 含义 |
|---|---|---|
| `DWMWA_WINDOW_CORNER_PREFERENCE` | 33 | 圆角偏好，设 `DWMWCP_ROUND`=2 |
| `DWMWA_SYSTEMBACKDROP_TYPE` | 38 | 背景类型，设 `DWMSBT_MAINWINDOW`=2（Mica） |
| `DWMWA_USE_IMMERSIVE_DARK_MODE` | 20 | 暗色标题栏，设 `prefersDarkTheme()` |

常量若 SDK 头未定义则带注释硬编码（项目用 MSVC 2022 + 新 SDK，大概率已有）。

系统主题读取：注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`（1=亮，0=暗）。

## CMake 与测试

- 新增 `WindowsWindowStyle.cpp/.h` 加入主程序 `PLATFORM_SOURCES`（Win32）与 `QtClickerMainWindowTests` 源列表。
- macOS 在 `PlatformServicesMac.mm` 提供 no-op。
- `dwmapi` 已链接到主程序和 MainWindow 测试，**无需新增链接依赖**。
- 可测单元：`isWindows11OrLater(quint32 build)` 纯函数加单测（build≥22000）。
- DWM 应用本身靠手动目视验证 + offscreen no-op 安全性保证。

## 验收标准

1. Win11 上主窗口呈现圆角 + Mica 透出效果，卡片保持白底，间隙透出云母；
2. Win10 上外观与现状一致，无回归；
3. macOS / offscreen 测试全绿，行为不变；
4. 切换窗口置顶后 DWM 属性正确保留（showEvent 重新应用）；
5. 全部既有测试通过（含 `usesClickFlowControlChrome`、控件高度断言）。
