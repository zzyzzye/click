# ClickFlow Fluent 浅色改造实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 Mica 黑背景与右缘渲染残留，完成 Fluent 浅色改造与沉浸式自定义标题栏。

**Architecture:** 设计规格见 `docs/superpowers/specs/2026-08-31-fluent-light-redesign-design.md`。单套浅色设计令牌（`ThemeTokens`）注入模板化 QSS；沉浸式标题栏用 WM_NCCALCSIZE 去非客户区（保留原生圆角/阴影/Snap/Mica）+ 自绘 `CaptionBar` 组件；Mica 修复通过 DWM 调用加固 + 句柄重建后重应用。

**Tech Stack:** C++20、Qt 6.8.3（Widgets/Test）、CMake + Visual Studio 18 2026、Win32 DWM API。

## Global Constraints

- 构建目录：`build/windows-vs2026-debug`（已配置好，无需重新 configure，除非改了 CMakeLists —— 改了就要加 `--fresh` 不需要，正常 reconfigure 会自动发生）
- 构建命令：`cmake --build build/windows-vs2026-debug --config Debug`
- 测试命令：`ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure`（可用 `-R <测试名>` 跑单个，如 `-R MainWindow`）
- Win32 专用代码必须用 `#if defined(Q_OS_WIN)` 保护——本项目同时构建 macOS
- 提交信息：简体中文 `类型：简要说明`（类型限 `功能/修复/重构/测试/文档/构建`），结尾另起一行加 `Co-Authored-By: Claude Code <noreply@anthropic.com>`
- 注释、日志、UI 文案用简体中文；技术术语与 API 名保留原文
- 仅浅色主题，禁止引入深色分支或主题切换逻辑
- 全部既有测试必须保持绿色（注意 Task 3 列出的两个 QSS 结构断言的精确前缀）
- 验收以 spec §7 的手动验证清单为准

## 文件结构

| 文件 | 职责 |
|---|---|
| `src/platform/windows/WindowsWindowStyle.{h,cpp}` | DWM 窗口样式：Mica backdrop、圆角、透明判定（加固） |
| `src/platform/WindowStyleService.h` | 平台接口（移除 prefersDarkTheme） |
| `src/platform/macos/PlatformServicesMac.mm` | macOS 桩实现（同步移除 prefersDarkTheme） |
| `src/app/UiStyle.{h,cpp}` | ThemeTokens + 模板化 Fluent QSS（全量重写） |
| `src/app/resources/check.svg` + `ClickFlowResources.qrc` | 复选框对勾资源 |
| `src/app/widgets/NavItemDelegate.{h,cpp}` | 侧边栏导航项绘制（选中指示条） |
| `src/app/widgets/CaptionBar.{h,cpp}` | 沉浸式标题栏组件（拖拽区 + 三个窗口控制按钮） |
| `src/app/MainWindow.{h,cpp}` | 装配 CaptionBar、nativeEvent（WM_NCCALCSIZE/WM_NCHITTEST）、句柄重建重应用 |
| `src/app/pages/ClickSettingsPage.cpp` | 补 alwaysOnTop 复选框 objectName（测试需要） |
| `tests/WindowsWindowStyleTests.cpp` | 新增纯函数测试 |
| `tests/UiStyleTests.cpp`（新） | 令牌与样式表单元测试 |
| `tests/ProductShellTests.cpp` | NavItemDelegate + CaptionBar 测试 |
| `tests/MainWindowTests.cpp` | 重应用测试 + 标题栏装配测试 |
| `CMakeLists.txt` | 新源文件与新测试目标注册 |

---

### Task 1: WindowsWindowStyle Mica 加固

**Files:**
- Modify: `src/platform/windows/WindowsWindowStyle.h`
- Modify: `src/platform/windows/WindowsWindowStyle.cpp`
- Modify: `src/platform/WindowStyleService.h`（移除 prefersDarkTheme）
- Modify: `src/platform/macos/PlatformServicesMac.mm:25`（同步移除）
- Test: `tests/WindowsWindowStyleTests.cpp`

**Interfaces:**
- Consumes: 无（首个任务）
- Produces:
  - `bool isWindows11OrLater(unsigned int buildNumber)`（既有，保持不变）
  - `bool supportsSystemBackdrop(unsigned int buildNumber)` — `buildNumber >= 22621`
  - `bool micaBackdropAvailable(unsigned int buildNumber, bool transparencyEnabled)` — Win11 && 支持 backdrop && 系统透明效果开启
  - `WindowStyleService` 接口变为：`prepare(QWidget*)` / `apply(QWidget*)` / `usesBackdrop() const`（三个纯虚函数，不再有 `prefersDarkTheme`）

**背景：** 当前 Mica 黑背景最可疑根因是 DWM 调用静默失败或属性随 HWND 重建丢失；且 `DWMWA_SYSTEMBACKDROP_TYPE` 实际需要 build ≥ 22621（22H2），22000 阈值过于宽松。本任务加固所有 DWM 调用并补上 `DwmExtendFrameIntoClientArea`（社区验证的 Qt Widgets + Mica 必要步骤），同时按新决策移除暗色分支。

- [ ] **Step 1: 写失败测试**

在 `tests/WindowsWindowStyleTests.cpp` 中，向 `private slots:` 追加三个声明，并追加实现：

```cpp
// private slots: 中追加
  void systemBackdropRequiresBuild22621();
  void micaBackdropRequiresTransparency();
  void windows11ThresholdUnchanged();
```

```cpp
void WindowsWindowStyleTests::systemBackdropRequiresBuild22621() {
  QVERIFY(supportsSystemBackdrop(22621));
  QVERIFY(supportsSystemBackdrop(26200));
  QVERIFY(!supportsSystemBackdrop(22620));
  QVERIFY(!supportsSystemBackdrop(22000));
}

void WindowsWindowStyleTests::micaBackdropRequiresTransparency() {
  // Mica 需要：Win11 + build>=22621 + 系统透明效果开启，三者缺一不可
  QVERIFY(micaBackdropAvailable(26200, true));
  QVERIFY(!micaBackdropAvailable(26200, false));
  QVERIFY(!micaBackdropAvailable(22620, true));
  QVERIFY(!micaBackdropAvailable(19045, true));
}

void WindowsWindowStyleTests::windows11ThresholdUnchanged() {
  QVERIFY(isWindows11OrLater(22000));
  QVERIFY(!isWindows11OrLater(21999));
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target WindowsWindowStyleTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R WindowsWindowStyle --output-on-failure
```

预期：编译失败（`supportsSystemBackdrop` / `micaBackdropAvailable` 未声明）。

- [ ] **Step 3: 修改 `src/platform/windows/WindowsWindowStyle.h`**

完整替换为：

```cpp
#pragma once

#include <memory>

#include "platform/WindowStyleService.h"

class QWidget;

// 纯函数，便于单元测试。传入注册表读到的 CurrentBuildNumber。
bool isWindows11OrLater(unsigned int buildNumber);

// DWMWA_SYSTEMBACKDROP_TYPE（Mica）自 Windows 11 22H2（build 22621）起可用。
bool supportsSystemBackdrop(unsigned int buildNumber);

// Mica 生效的全部条件：Win11、build 支持 backdrop、系统「透明效果」开启。
bool micaBackdropAvailable(unsigned int buildNumber, bool transparencyEnabled);

class WindowsWindowStyle final : public WindowStyleService {
 public:
  void prepare(QWidget* window) override;
  void apply(QWidget* window) override;
  bool usesBackdrop() const override;
};
```

- [ ] **Step 4: 修改 `src/platform/WindowStyleService.h`（移除 prefersDarkTheme）**

```cpp
#pragma once

#include <memory>

class QWidget;

class WindowStyleService {
 public:
  virtual ~WindowStyleService() = default;
  virtual void prepare(QWidget* window) = 0;
  virtual void apply(QWidget* window) = 0;
  virtual bool usesBackdrop() const = 0;
};

std::unique_ptr<WindowStyleService> createWindowStyleService();
```

同步修改 `src/platform/macos/PlatformServicesMac.mm:25`：删除其中的 `bool prefersDarkTheme() const override { return false; }` 一行。

- [ ] **Step 5: 重写 `src/platform/windows/WindowsWindowStyle.cpp`**

```cpp
#include "platform/windows/WindowsWindowStyle.h"

#include <QDebug>
#include <QString>
#include <QWidget>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dwmapi.h>

namespace {

// 若 SDK 头未提供这些常量则回退硬编码（MSVC 2022 + 新 SDK 通常已有）。
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

constexpr int kDwmwcpRound = 2;       // DWMWCP_ROUND
constexpr int kDwmsbtMainWindow = 2;  // DWMSBT_MAINWINDOW（Mica）

unsigned int currentBuildNumber() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                    KEY_READ, &key) != ERROR_SUCCESS) {
    return 0;
  }
  wchar_t buffer[32] = {};
  DWORD size = sizeof(buffer);
  const LSTATUS status =
      RegQueryValueExW(key, L"CurrentBuildNumber", nullptr, nullptr,
                       reinterpret_cast<LPBYTE>(buffer), &size);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS) return 0;
  bool ok = false;
  const unsigned int build = QString::fromWCharArray(buffer).toUInt(&ok);
  return ok ? build : 0;
}

// 系统「设置 → 个性化 → 颜色 → 透明效果」。关闭时 Windows 不绘制 Mica。
bool readTransparencyEnabled() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0, KEY_READ, &key) != ERROR_SUCCESS) {
    return true;  // 默认开启
  }
  DWORD value = 1;
  DWORD size = sizeof(value);
  RegQueryValueExW(key, L"EnableTransparency", nullptr, nullptr,
                   reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(key);
  return value != 0;
}

HWND widgetHandle(const QWidget* widget) {
  return reinterpret_cast<HWND>(widget->winId());
}

void applyDwmAttribute(HWND handle, DWORD attribute, const void* value,
                       DWORD size, const char* description) {
  const HRESULT result = DwmSetWindowAttribute(handle, attribute, value, size);
  if (FAILED(result)) {
    qWarning() << "DWM 设置失败:" << description
               << "HRESULT =" << Qt::hex << static_cast<quulonglong>(result);
  }
}

}  // namespace

bool isWindows11OrLater(unsigned int buildNumber) {
  return buildNumber >= 22000;
}

bool supportsSystemBackdrop(unsigned int buildNumber) {
  return buildNumber >= 22621;
}

bool micaBackdropAvailable(unsigned int buildNumber, bool transparencyEnabled) {
  return isWindows11OrLater(buildNumber) &&
         supportsSystemBackdrop(buildNumber) && transparencyEnabled;
}

bool WindowsWindowStyle::usesBackdrop() const {
  return micaBackdropAvailable(currentBuildNumber(), readTransparencyEnabled());
}

void WindowsWindowStyle::prepare(QWidget* window) {
  if (!window) return;
  // 仅在 Mica 确定可用时才透明化窗口，否则透明区域会直接透出桌面。
  if (usesBackdrop()) {
    window->setAttribute(Qt::WA_TranslucentBackground);
  }
}

void WindowsWindowStyle::apply(QWidget* window) {
  if (!window) return;
  if (!usesBackdrop()) return;

  HWND handle = widgetHandle(window);
  if (!handle) return;

  const int corner = kDwmwcpRound;
  applyDwmAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                    sizeof(corner), "窗口圆角");

  const int backdrop = kDwmsbtMainWindow;
  applyDwmAttribute(handle, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop,
                    sizeof(backdrop), "Mica 背景");

  // 仅浅色主题：始终关闭沉浸式暗色标题栏。
  const BOOL dark = FALSE;
  applyDwmAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark),
                    "暗色标题栏");

  // 将 DWM 框架扩展到整个客户区，确保透明区域绘制 Mica 材质而非透出桌面。
  const MARGINS margins{-1, -1, -1, -1};
  const HRESULT marginsResult = DwmExtendFrameIntoClientArea(handle, &margins);
  if (FAILED(marginsResult)) {
    qWarning() << "DWM 框架扩展失败: HRESULT =" << Qt::hex
               << static_cast<quulonglong>(marginsResult);
  }
}
```

注意：原 `readAppsUseLightTheme()` 函数随之删除。

- [ ] **Step 6: 跑测试确认通过**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target WindowsWindowStyleTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R WindowsWindowStyle --output-on-failure
```

预期：全部通过（含既有 3 个 + 新增 3 个用例）。

- [ ] **Step 7: 构建主程序与 MainWindow 测试，确认接口移除无遗漏**

```bash
cmake --build build/windows-vs2026-debug --config Debug
ctest --test-dir build/windows-vs2026-debug -C Debug -R MainWindow --output-on-failure
```

预期：编译链接成功（若有其他 `prefersDarkTheme` 引用会在此暴露，逐个删除），MainWindow 测试通过。

- [ ] **Step 8: Commit**

```bash
git add src/platform tests/WindowsWindowStyleTests.cpp
git commit -m "修复：加固 Windows Mica 窗口样式应用

- DWM 调用检查返回值并输出日志
- 补充 DwmExtendFrameIntoClientArea 确保透明区绘制 Mica
- backdrop 阈值修正为 build 22621，检测系统透明效果
- 仅浅色主题，移除暗色分支

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 2: 窗口置顶切换后重应用窗口样式

**Files:**
- Modify: `src/app/MainWindow.cpp:792`（applyWindowOnTop）
- Modify: `src/app/pages/ClickSettingsPage.cpp:91`（补 objectName）
- Test: `tests/MainWindowTests.cpp`

**Interfaces:**
- Consumes: Task 1 的 `WindowStyleService` 三方法接口
- Produces: `ClickSettingsPage` 的置顶复选框 objectName 为 `"alwaysOnTopCheckBox"`（后续任务与测试可用）

**背景：** `setWindowFlag(Qt::WindowStaysOnTopHint)` 销毁并重建 HWND，DWM 属性（圆角/Mica/框架扩展）全部丢失，这是黑背景的首要嫌疑路径。修复：每次切换后重新 `apply()`。

- [ ] **Step 1: 写失败测试**

在 `tests/MainWindowTests.cpp` 顶部假服务区域（`MainWindowFakeMacroPlayer` 之后）追加：

```cpp
class MainWindowFakeWindowStyle final : public WindowStyleService {
 public:
  void prepare(QWidget*) override { ++prepareCount; }
  void apply(QWidget*) override { ++applyCount; }
  bool usesBackdrop() const override { return false; }
  int prepareCount = 0;
  int applyCount = 0;
};
```

在 `private slots:` 追加 `void windowStyleReappliedAfterOnTopToggle();`，实现：

```cpp
void MainWindowTests::windowStyleReappliedAfterOnTopToggle() {
  const QString appName =
      QString("QtClickerMainWindowStyleTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  auto windowStyle = std::make_unique<MainWindowFakeWindowStyle>();
  auto* observed = windowStyle.get();

  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository), MacroPlatformServices{}, nullptr,
                    [](QWidget*) { return true; },
                    [](QWidget*) { return QString("测试"); },
                    std::move(windowStyle));

  const int before = observed->applyCount;
  auto* onTop = window.findChild<QCheckBox*>("alwaysOnTopCheckBox");
  QVERIFY(onTop);
  onTop->click();
  QCOMPARE(observed->applyCount, before + 1);
  onTop->click();
  QCOMPARE(observed->applyCount, before + 2);
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R MainWindow --output-on-failure
```

预期：失败——`alwaysOnTopCheckBox` 找不到（QVERIFY(onTop) 失败）。

- [ ] **Step 3: 补 objectName**

`src/app/pages/ClickSettingsPage.cpp:91` 处，在 `alwaysOnTop_ = new QCheckBox("保持窗口置顶", this);` 之后加一行：

```cpp
  alwaysOnTop_->setObjectName("alwaysOnTopCheckBox");
```

- [ ] **Step 4: 修改 applyWindowOnTop**

`src/app/MainWindow.cpp:792`，把：

```cpp
void MainWindow::applyWindowOnTop(bool enabled) { const bool shown = isVisible(); setWindowFlag(Qt::WindowStaysOnTopHint, enabled); if (shown) { show(); raise(); } }
```

改为：

```cpp
void MainWindow::applyWindowOnTop(bool enabled) {
  const bool shown = isVisible();
  setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
  if (shown) {
    show();
    raise();
  }
  // setWindowFlag 会销毁并重建 HWND，DWM 属性（圆角/Mica/框架扩展）随之丢失，必须重新应用。
  windowStyle_->apply(this);
}
```

- [ ] **Step 5: 跑测试确认通过**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R MainWindow --output-on-failure
```

预期：全部通过。

- [ ] **Step 6: Commit**

```bash
git add src/app/MainWindow.cpp src/app/pages/ClickSettingsPage.cpp tests/MainWindowTests.cpp
git commit -m "修复：窗口置顶切换后重新应用 Mica 窗口样式

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 3: ThemeTokens + Fluent 浅色 QSS 全量重写

**Files:**
- Modify: `src/app/UiStyle.h`（全量替换）
- Modify: `src/app/UiStyle.cpp`（全量替换）
- Create: `src/app/resources/check.svg`
- Modify: `src/app/resources/ClickFlowResources.qrc`
- Create: `tests/UiStyleTests.cpp`
- Modify: `CMakeLists.txt`（注册 ClickFlowUiStyleTests）

**Interfaces:**
- Consumes: 无（独立任务，但 MainWindow 已调用 `clickFlowStyleSheet(bool)`，签名保持不变）
- Produces:
  - `struct ThemeTokens`：字段 `accent / accentHover / accentPressed / textPrimary(#1B1B1B) / textSecondary(#616161) / textDisabled(#9E9E9E) / danger(#C42B1C) / dangerHover(#B7271C) / dangerPressed(#A5231B) / controlBackground(#FDFDFD) / controlBackgroundHover(#F9F9F9) / controlBackgroundPressed(#F5F5F5) / navItemHover / navItemSelected / controlRadius(4) / cardRadius(8) / controlHeight(32)`
  - `const ThemeTokens& fluentLightTokens();` — 全局唯一浅色令牌（Task 4、5 的颜色来源）
  - `QString clickFlowStyleSheet(bool translucent = false);` — 签名不变

**既有测试的精确前缀（必须保留，否则 MainWindowTests 红）：**
- `QMainWindow, #contentSurface { background: transparent`（simplified 后）
- `#contentPages { background: transparent`
- `#settingsCard, #statusStrip, #actionBar { background: white`
- `#sidebarNavigation { background: transparent; border: none;`
- 字符串 `QComboBox::down-arrow`、`QSpinBox::up-button`、`QSpinBox::down-button`、`:/clickflow/icons/chevron-down.svg`、`:/clickflow/icons/chevron-up.svg`

- [ ] **Step 1: 写失败测试**

创建 `tests/UiStyleTests.cpp`：

```cpp
#include <QTest>

#include "app/UiStyle.h"

class UiStyleTests : public QObject {
  Q_OBJECT

 private slots:
  void tokensAreFluentLight();
  void translucentSurfacesAreTransparent();
  void opaqueSurfacesUseFallback();
  void fluentControlMetrics();
  void controlChromePreserved();
};

void UiStyleTests::tokensAreFluentLight() {
  const ThemeTokens& tokens = fluentLightTokens();
  QCOMPARE(tokens.accent, QColor("#0067C0"));
  QCOMPARE(tokens.textPrimary, QColor("#1B1B1B"));
  QCOMPARE(tokens.danger, QColor("#C42B1C"));
  QCOMPARE(tokens.controlRadius, 4);
  QCOMPARE(tokens.cardRadius, 8);
  QCOMPARE(tokens.controlHeight, 32);
}

void UiStyleTests::translucentSurfacesAreTransparent() {
  const QString style = clickFlowStyleSheet(true).simplified();
  QVERIFY(style.contains("QMainWindow, #contentSurface { background: transparent"));
  QVERIFY(style.contains("#contentPages { background: transparent"));
  QVERIFY(style.contains("#navigationSidebar { background: transparent"));
}

void UiStyleTests::opaqueSurfacesUseFallback() {
  const QString style = clickFlowStyleSheet(false).simplified();
  QVERIFY(style.contains("QMainWindow, #contentSurface { background: #F3F3F3"));
  QVERIFY(style.contains("#contentPages { background: #F3F3F3"));
  QVERIFY(style.contains("#navigationSidebar { background: #F3F3F3"));
  // 回退模式下卡片保持纯白
  QVERIFY(style.contains("#settingsCard, #statusStrip, #actionBar { background: white"));
}

void UiStyleTests::fluentControlMetrics() {
  const QString style = clickFlowStyleSheet(true);
  QVERIFY(style.contains("min-height: 32px"));
  QVERIFY(style.contains("border-radius: 4px"));
  QVERIFY(style.contains("#0067C0"));
  QVERIFY(style.contains("#C42B1C"));
}

void UiStyleTests::controlChromePreserved() {
  const QString style = clickFlowStyleSheet(true);
  QVERIFY(style.contains("QComboBox::down-arrow"));
  QVERIFY(style.contains("QSpinBox::up-button"));
  QVERIFY(style.contains("QSpinBox::down-button"));
  QVERIFY(style.contains(":/clickflow/icons/chevron-down.svg"));
  QVERIFY(style.contains(":/clickflow/icons/chevron-up.svg"));
  QVERIFY(style.contains(":/clickflow/icons/check.svg"));
}

QTEST_APPLESS_MAIN(UiStyleTests)
#include "UiStyleTests.moc"
```

在 `CMakeLists.txt` 的 `add_test(NAME ProductShell ...)` 块（约 263 行）之后插入新测试目标：

```cmake
add_executable(ClickFlowUiStyleTests
  tests/UiStyleTests.cpp
  src/app/UiStyle.cpp
  src/app/UiStyle.h
)
target_include_directories(ClickFlowUiStyleTests PRIVATE src)
target_link_libraries(ClickFlowUiStyleTests PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME UiStyle COMMAND ClickFlowUiStyleTests)
if(WIN32)
  set_tests_properties(UiStyle PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowUiStyleTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R UiStyle --output-on-failure
```

预期：编译失败（`ThemeTokens` / `fluentLightTokens` 未定义）。

- [ ] **Step 3: 重写 `src/app/UiStyle.h`**

```cpp
#pragma once

#include <QColor>
#include <QString>

// Fluent 浅色设计令牌：全部 UI 颜色、圆角、控件高度的唯一来源。
struct ThemeTokens {
  QColor accent;                    // #0067C0
  QColor accentHover;               // #1975C5
  QColor accentPressed;             // #1669B5
  QColor textPrimary;               // #1B1B1B
  QColor textSecondary;             // #616161
  QColor textDisabled;              // #9E9E9E
  QColor danger;                    // #C42B1C
  QColor dangerHover;               // #B7271C
  QColor dangerPressed;             // #A5231B
  QColor controlBackground;         // #FDFDFD
  QColor controlBackgroundHover;    // #F9F9F9
  QColor controlBackgroundPressed;  // #F5F5F5
  QColor navItemHover;              // rgba(255,255,255,0.50)
  QColor navItemSelected;           // rgba(255,255,255,0.70)
  int controlRadius = 4;
  int cardRadius = 8;
  int controlHeight = 32;
};

// 全局唯一浅色令牌实例。
const ThemeTokens& fluentLightTokens();

// translucent=true 时窗口与侧边栏背景透明（透出 Mica），否则回退不透明浅色。
QString clickFlowStyleSheet(bool translucent = false);
```

- [ ] **Step 4: 新增复选框对勾资源**

创建 `src/app/resources/check.svg`：

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="12" height="10" viewBox="0 0 12 10"><path d="M1 5.5 4.5 9 11 1" fill="none" stroke="#FFFFFF" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>
```

修改 `src/app/resources/ClickFlowResources.qrc`，在 `<qresource prefix="/clickflow/icons">` 内追加一行：

```xml
    <file alias="check.svg">check.svg</file>
```

- [ ] **Step 5: 重写 `src/app/UiStyle.cpp`**

```cpp
#include "app/UiStyle.h"

namespace {

QString hex(const QColor& color) { return color.name(); }

QString rgba(int r, int g, int b, double a) {
  return QStringLiteral("rgba(%1,%2,%3,%4)")
      .arg(r)
      .arg(g)
      .arg(b)
      .arg(a, 0, 'f', 2);
}

}  // namespace

const ThemeTokens& fluentLightTokens() {
  static const ThemeTokens tokens{
      QColor("#0067C0"),            // accent
      QColor("#1975C5"),            // accentHover
      QColor("#1669B5"),            // accentPressed
      QColor("#1B1B1B"),            // textPrimary
      QColor("#616161"),            // textSecondary
      QColor("#9E9E9E"),            // textDisabled
      QColor("#C42B1C"),            // danger
      QColor("#B7271C"),            // dangerHover
      QColor("#A5231B"),            // dangerPressed
      QColor("#FDFDFD"),            // controlBackground
      QColor("#F9F9F9"),            // controlBackgroundHover
      QColor("#F5F5F5"),            // controlBackgroundPressed
      QColor(255, 255, 255, 128),   // navItemHover
      QColor(255, 255, 255, 178),   // navItemSelected
      4,                            // controlRadius
      8,                            // cardRadius
      32,                           // controlHeight
  };
  return tokens;
}

QString clickFlowStyleSheet(bool translucent) {
  const ThemeTokens& t = fluentLightTokens();
  const char* surfaceBackground = translucent ? "transparent" : "#F3F3F3";
  const char* contentBackground = translucent ? "transparent" : "#F3F3F3";
  const char* sidebarBackground = translucent ? "transparent" : "#F3F3F3";
  return QStringLiteral(R"(
    QWidget {
      font-family: "Segoe UI Variable Text", "Segoe UI", "Microsoft YaHei UI";
      font-size: 13px;
    }
    QMainWindow, #contentSurface { background: %1; color: %4; }
    #contentPages { background: %2; }
    QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
    #navigationSidebar { background: %3; }
    #productName { font-size: 20px; font-weight: 600; color: %4; }
    #productVersion { color: %5; font-size: 12px; }
    #sidebarNavigation { background: transparent; border: none; outline: none; }
    #sidebarNavigation::item { border-radius: 4px; padding-left: 12px; }
    #sidebarNavigation::item:hover { background: rgba(255,255,255,0.50); }
    #sidebarNavigation::item:selected { background: rgba(255,255,255,0.70); color: %4; }
    #settingsCard, #statusStrip, #actionBar {
      background: white; border: 1px solid rgba(0,0,0,0.08); border-radius: 8px;
    }
    #cardTitle { font-size: 14px; font-weight: 600; }
    QPushButton {
      min-height: 32px; max-height: 32px;
      background: %6; color: %4;
      border: 1px solid rgba(0,0,0,0.08);
      border-bottom: 1px solid rgba(0,0,0,0.16);
      border-radius: 4px;
      padding: 0 12px;
    }
    QPushButton:hover { background: %7; }
    QPushButton:pressed { background: %8; color: %5; }
    QPushButton:focus { border-color: %4; }
    QPushButton:disabled {
      color: %9; background: rgba(0,0,0,0.04); border-color: rgba(0,0,0,0.04);
    }
    QPushButton#startStopButton,
    QPushButton#macroRecordButton,
    QPushButton#macroPlayButton {
      min-height: 36px; max-height: 36px;
      color: white;
      border: 1px solid rgba(255,255,255,0.08);
      border-bottom: 1px solid rgba(0,0,0,0.40);
      border-radius: 4px;
      padding: 0 16px; font-weight: 600;
    }
    QPushButton#startStopButton,
    QPushButton#macroRecordButton,
    QPushButton#macroPlayButton { background: %10; }
    QPushButton#startStopButton:hover,
    QPushButton#macroRecordButton:hover,
    QPushButton#macroPlayButton:hover { background: %11; }
    QPushButton#startStopButton:pressed,
    QPushButton#macroRecordButton:pressed,
    QPushButton#macroPlayButton:pressed { background: %12; }
    QPushButton#startStopButton:disabled,
    QPushButton#macroRecordButton:disabled,
    QPushButton#macroPlayButton:disabled {
      color: %9; background: rgba(0,0,0,0.04);
    }
    QPushButton#startStopButton[running="true"] { background: %13; }
    QPushButton#startStopButton[running="true"]:hover { background: %14; }
    QPushButton#startStopButton[running="true"]:pressed { background: %15; }
    QComboBox, QSpinBox, QKeySequenceEdit, QLineEdit {
      min-height: 32px; max-height: 32px;
      border: 1px solid rgba(0,0,0,0.08);
      border-bottom: 1px solid rgba(0,0,0,0.42);
      border-radius: 4px;
      background: %6; padding: 0 34px 0 10px;
      selection-color: white; selection-background-color: %10;
    }
    QLineEdit { padding: 0 10px; }
    QComboBox:hover, QSpinBox:hover, QKeySequenceEdit:hover, QLineEdit:hover {
      background: %7;
    }
    QComboBox:focus, QSpinBox:focus, QKeySequenceEdit:focus, QLineEdit:focus {
      background: white; border: 1px solid %10; border-bottom: 2px solid %10;
    }
    QComboBox:disabled, QSpinBox:disabled, QKeySequenceEdit:disabled, QLineEdit:disabled {
      color: %9; background: rgba(0,0,0,0.04);
      border-bottom-color: rgba(0,0,0,0.08);
    }
    QComboBox::drop-down {
      subcontrol-origin: padding; subcontrol-position: top right;
      width: 30px; margin: 3px; border: none; border-radius: 4px;
    }
    QComboBox::drop-down:hover { background: rgba(0,0,0,0.06); }
    QComboBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.svg);
      width: 12px; height: 8px;
    }
    QComboBox QAbstractItemView {
      background: white; border: 1px solid rgba(0,0,0,0.08);
      border-radius: 8px; padding: 4px; outline: none;
    }
    QComboBox QAbstractItemView::item {
      min-height: 28px; border-radius: 4px; padding-left: 10px;
    }
    QComboBox QAbstractItemView::item:hover { background: rgba(0,0,0,0.04); }
    QComboBox QAbstractItemView::item:selected {
      background: rgba(0,0,0,0.06); color: %4;
    }
    QSpinBox { padding-right: 32px; }
    QSpinBox::up-button, QSpinBox::down-button {
      subcontrol-origin: border; width: 28px;
      border: none; background: transparent;
    }
    QSpinBox::up-button {
      subcontrol-position: top right; margin: 3px 3px 0 0;
      border-top-left-radius: 4px; border-top-right-radius: 4px;
    }
    QSpinBox::down-button {
      subcontrol-position: bottom right; margin: 0 3px 3px 0;
      border-bottom-left-radius: 4px; border-bottom-right-radius: 4px;
    }
    QSpinBox::up-button:hover, QSpinBox::down-button:hover {
      background: rgba(0,0,0,0.06);
    }
    QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
      background: rgba(0,0,0,0.10);
    }
    QSpinBox::up-arrow {
      image: url(:/clickflow/icons/chevron-up.svg);
      width: 10px; height: 6px;
    }
    QSpinBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.svg);
      width: 10px; height: 6px;
    }
    QCheckBox { spacing: 8px; }
    QCheckBox::indicator {
      width: 18px; height: 18px;
      border: 1px solid rgba(0,0,0,0.45); border-radius: 4px;
      background: white;
    }
    QCheckBox::indicator:hover { border-color: rgba(0,0,0,0.60); }
    QCheckBox::indicator:checked {
      background: %10; border-color: %10;
      image: url(:/clickflow/icons/check.svg);
    }
    QCheckBox::indicator:checked:hover {
      background: %11; border-color: %11;
    }
    QCheckBox::indicator:disabled {
      background: rgba(0,0,0,0.04); border-color: rgba(0,0,0,0.20);
    }
    QListWidget { outline: none; }
    QListWidget::item { border-radius: 4px; }
    QListWidget::item:hover { background: rgba(0,0,0,0.04); }
    QListWidget::item:selected { background: rgba(0,0,0,0.06); color: %4; }
    QScrollBar:vertical {
      background: transparent; width: 12px; margin: 2px 2px 2px 0;
    }
    QScrollBar::handle:vertical {
      background: rgba(0,0,0,0.35); min-height: 32px;
      border-radius: 3px; margin: 0 3px;
    }
    QScrollBar::handle:vertical:hover { background: rgba(0,0,0,0.55); }
    QScrollBar::handle:vertical:pressed { background: rgba(0,0,0,0.65); }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      height: 0; border: none; background: transparent;
    }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
      background: transparent;
    }
    QScrollBar:horizontal {
      background: transparent; height: 12px; margin: 0 2px 2px 2px;
    }
    QScrollBar::handle:horizontal {
      background: rgba(0,0,0,0.35); min-width: 32px;
      border-radius: 3px; margin: 3px 0;
    }
    QScrollBar::handle:horizontal:hover { background: rgba(0,0,0,0.55); }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
      width: 0; border: none; background: transparent;
    }
    QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
      background: transparent;
    }
    QToolTip {
      background: white; color: %4;
      border: 1px solid rgba(0,0,0,0.10); border-radius: 4px;
      padding: 6px 10px;
    }
    QMenu {
      background: white; border: 1px solid rgba(0,0,0,0.08);
      border-radius: 8px; padding: 4px;
    }
    QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }
    QMenu::item:selected { background: rgba(0,0,0,0.06); }
  )")
      .arg(QString::fromLatin1(surfaceBackground),
           QString::fromLatin1(contentBackground),
           QString::fromLatin1(sidebarBackground),
           hex(t.textPrimary),           // %4
           hex(t.textSecondary),         // %5
           hex(t.controlBackground),     // %6
           hex(t.controlBackgroundHover),     // %7
           hex(t.controlBackgroundPressed),   // %8
           hex(t.textDisabled),          // %9
           hex(t.accent),                // %10
           hex(t.accentHover),           // %11
           hex(t.accentPressed),         // %12
           hex(t.danger),                // %13
           hex(t.dangerHover),           // %14
           hex(t.dangerPressed));        // %15
}
```

- [ ] **Step 6: 跑 UiStyle 与 MainWindow 测试确认通过**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowUiStyleTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R UiStyle --output-on-failure
cmake --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R MainWindow --output-on-failure
```

预期：UiStyle 全过；MainWindow 的 `usesClickFlowControlChrome` 与 `transparentStyleSheetKeepsCardsOpaque` 保持绿色（精确前缀已在上方约束中列出）。

- [ ] **Step 7: 全量回归**

```bash
cmake --build build/windows-vs2026-debug --config Debug
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

预期：全部通过。

- [ ] **Step 8: Commit**

```bash
git add src/app/UiStyle.h src/app/UiStyle.cpp src/app/resources tests/UiStyleTests.cpp CMakeLists.txt
git commit -m "功能：Fluent 浅色设计令牌与样式表全量重写

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 4: 侧边栏导航 Fluent 选中指示条

**Files:**
- Create: `src/app/widgets/NavItemDelegate.h`
- Create: `src/app/widgets/NavItemDelegate.cpp`
- Modify: `src/app/widgets/NavigationSidebar.cpp`（挂 delegate、行高 42→36）
- Modify: `CMakeLists.txt`（SHELL_WIDGET_SOURCES 加新文件；ProductShellTests 补 UiStyle.cpp）
- Test: `tests/ProductShellTests.cpp`

**Interfaces:**
- Consumes: Task 3 的 `fluentLightTokens()` / `ThemeTokens`（accent、navItemHover、navItemSelected、textPrimary、controlRadius）
- Produces: `NavItemDelegate`（QStyledItemDelegate 子类，`using QStyledItemDelegate::QStyledItemDelegate;`，重写 `paint` 与 `sizeHint`）

**背景：** Fluent 导航选中态是「浅灰底 + 左侧 3px accent 指示条（高 16px 垂直居中）」，QSS 画不出居中短指示条，需要自绘 delegate。设置 delegate 后 QSS 的 `::item` 规则不再参与绘制，delegate 全权负责。

- [ ] **Step 1: 写失败测试**

在 `tests/ProductShellTests.cpp` 顶部追加 include：

```cpp
#include <QPainter>

#include "app/UiStyle.h"
#include "app/widgets/NavItemDelegate.h"
```

`private slots:` 追加两个声明，实现：

```cpp
void ProductShellTests::navDelegatePaintsFluentSelectionIndicator() {
  QListWidget list;
  auto* delegate = new NavItemDelegate(&list);
  list.setItemDelegate(delegate);
  new QListWidgetItem(QStringLiteral("连点设置"), &list);

  QPixmap canvas(160, 36);
  canvas.fill(Qt::transparent);
  QPainter painter(&canvas);
  QStyleOptionViewItem option;
  option.rect = QRect(0, 0, 160, 36);
  option.state = QStyle::State_Enabled | QStyle::State_Selected;
  delegate->paint(&painter, option, list.model()->index(0, 0));
  painter.end();

  // 指示条：左侧 x+4、宽 3、高 16、垂直居中 → 中心采样点 (5, 18)
  const QColor indicator = canvas.toImage().pixelColor(5, 18);
  const QColor accent = fluentLightTokens().accent;
  QCOMPARE(indicator.red(), accent.red());
  QCOMPARE(indicator.green(), accent.green());
  QCOMPARE(indicator.blue(), accent.blue());
}

void ProductShellTests::navDelegateUsesFluentRowHeight() {
  QListWidget list;
  auto* delegate = new NavItemDelegate(&list);
  new QListWidgetItem(QStringLiteral("连点设置"), &list);
  QStyleOptionViewItem option;
  QCOMPARE(delegate->sizeHint(option, list.model()->index(0, 0)).height(), 36);
}
```

在 `CMakeLists.txt` 的 `set(SHELL_WIDGET_SOURCES ...)` 列表中（NavigationSidebar.h 之后）追加：

```cmake
  src/app/widgets/NavItemDelegate.cpp
  src/app/widgets/NavItemDelegate.h
```

并在 `add_executable(ClickFlowProductShellTests ...)` 的源列表中追加 `src/app/UiStyle.cpp` 与 `src/app/UiStyle.h`（NavItemDelegate 依赖 fluentLightTokens）。

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowProductShellTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R ProductShell --output-on-failure
```

预期：编译失败（NavItemDelegate.h 不存在）。

- [ ] **Step 3: 创建 `src/app/widgets/NavItemDelegate.h`**

```cpp
#pragma once

#include <QStyledItemDelegate>

// 侧边栏导航项绘制：Fluent 悬停/选中背景 + 左侧 3px accent 选中指示条。
class NavItemDelegate final : public QStyledItemDelegate {
  Q_OBJECT
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
};
```

- [ ] **Step 4: 创建 `src/app/widgets/NavItemDelegate.cpp`**

```cpp
#include "app/widgets/NavItemDelegate.h"

#include <QPainter>
#include <QTextOption>

#include "app/UiStyle.h"

void NavItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  const ThemeTokens& tokens = fluentLightTokens();
  const bool selected = option.state & QStyle::State_Selected;
  const bool hovered = option.state & QStyle::State_MouseOver;

  const QRectF rowRect = option.rect;
  painter->setPen(Qt::NoPen);
  if (selected) {
    painter->setBrush(tokens.navItemSelected);
    painter->drawRoundedRect(rowRect, tokens.controlRadius, tokens.controlRadius);
    // 左侧 3px accent 指示条，高 16px，垂直居中。
    const QRectF indicator(rowRect.left() + 4, rowRect.center().y() - 8, 3, 16);
    painter->setBrush(tokens.accent);
    painter->drawRoundedRect(indicator, 1.5, 1.5);
  } else if (hovered) {
    painter->setBrush(tokens.navItemHover);
    painter->drawRoundedRect(rowRect, tokens.controlRadius, tokens.controlRadius);
  }

  QTextOption textOption;
  textOption.setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  painter->setPen(tokens.textPrimary);
  const QRectF textRect = rowRect.adjusted(12, 0, -8, 0);
  painter->drawText(textRect, index.data(Qt::DisplayRole).toString(), textOption);
  painter->restore();
}

QSize NavItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
  return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 36);
}
```

- [ ] **Step 5: 挂载 delegate**

`src/app/widgets/NavigationSidebar.cpp`：

- 顶部追加 `#include "app/widgets/NavItemDelegate.h"`
- 在 `navigation_->setSpacing(4);` 之后追加：`navigation_->setItemDelegate(new NavItemDelegate(navigation_));`
- 行高：`row->setSizeHint(QSize(0, 42));` 改为 `row->setSizeHint(QSize(0, 36));`

- [ ] **Step 6: 跑测试确认通过 + 全量回归**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowProductShellTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R ProductShell --output-on-failure
cmake --build build/windows-vs2026-debug --config Debug
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

预期：全部通过。

- [ ] **Step 7: Commit**

```bash
git add src/app/widgets/NavItemDelegate.h src/app/widgets/NavItemDelegate.cpp src/app/widgets/NavigationSidebar.cpp tests/ProductShellTests.cpp CMakeLists.txt
git commit -m "功能：侧边栏导航改为 Fluent 选中指示条样式

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 5: CaptionBar 沉浸式标题栏组件

**Files:**
- Create: `src/app/widgets/CaptionBar.h`
- Create: `src/app/widgets/CaptionBar.cpp`
- Modify: `CMakeLists.txt`（SHELL_WIDGET_SOURCES 加新文件）
- Test: `tests/ProductShellTests.cpp`

**Interfaces:**
- Consumes: Task 3 的 `fluentLightTokens()`（textPrimary、danger）
- Produces（Task 6 依赖，签名必须一致）:
  - `CaptionBar::CaptionBar(const QString& title, QWidget* parent = nullptr)`
  - 信号：`void minimizeRequested(); void maximizeRestoreRequested(); void closeRequested();`
  - `void setMaximized(bool maximized); bool isMaximized() const;`
  - `void setMaximizeButtonHovered(bool hovered);`（Task 6 的 WM_NCMOUSEMOVE 同步用）
  - 子按钮 objectName：`"captionMinimizeButton"` / `"captionMaximizeButton"` / `"captionCloseButton"`
  - 固定高度 32（`setFixedHeight(32)`）

- [ ] **Step 1: 写失败测试**

`tests/ProductShellTests.cpp` 追加 include `#include "app/widgets/CaptionBar.h"`，`private slots:` 追加声明，实现：

```cpp
void ProductShellTests::captionBarExposesWindowControls() {
  CaptionBar bar(QStringLiteral("ClickFlow"));
  QCOMPARE(bar.sizeHint().height(), 32);

  QSignalSpy minimizeSpy(&bar, &CaptionBar::minimizeRequested);
  QSignalSpy maximizeSpy(&bar, &CaptionBar::maximizeRestoreRequested);
  QSignalSpy closeSpy(&bar, &CaptionBar::closeRequested);

  auto* minButton = bar.findChild<QAbstractButton*>("captionMinimizeButton");
  auto* maxButton = bar.findChild<QAbstractButton*>("captionMaximizeButton");
  auto* closeButton = bar.findChild<QAbstractButton*>("captionCloseButton");
  QVERIFY(minButton);
  QVERIFY(maxButton);
  QVERIFY(closeButton);

  minButton->click();
  maxButton->click();
  closeButton->click();
  QCOMPARE(minimizeSpy.count(), 1);
  QCOMPARE(maximizeSpy.count(), 1);
  QCOMPARE(closeSpy.count(), 1);

  QVERIFY(!bar.isMaximized());
  bar.setMaximized(true);
  QVERIFY(bar.isMaximized());
  bar.setMaximizeButtonHovered(true);   // 不应崩溃（Task 6 的悬停同步入口）
  bar.setMaximizeButtonHovered(false);
}
```

在 `CMakeLists.txt` 的 `SHELL_WIDGET_SOURCES` 中追加：

```cmake
  src/app/widgets/CaptionBar.cpp
  src/app/widgets/CaptionBar.h
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowProductShellTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R ProductShell --output-on-failure
```

预期：编译失败（CaptionBar.h 不存在）。

- [ ] **Step 3: 创建 `src/app/widgets/CaptionBar.h`**

```cpp
#pragma once

#include <QAbstractButton>
#include <QWidget>

class QLabel;

// 沉浸式标题栏：左侧窗口标题，右侧最小化/最大化/关闭按钮，
// 空白区域拖动窗口、双击切换最大化。配合 MainWindow 的 WM_NCCALCSIZE 使用。
class CaptionBar final : public QWidget {
  Q_OBJECT
 public:
  explicit CaptionBar(const QString& title, QWidget* parent = nullptr);

  void setMaximized(bool maximized);
  bool isMaximized() const { return maximized_; }

  // WM_NCHITTEST 返回 HTMAXBUTTON 后按钮收不到普通 hover 事件，
  // 由 MainWindow 的 WM_NCMOUSEMOVE 手动同步悬停态。
  void setMaximizeButtonHovered(bool hovered);

  QSize sizeHint() const override { return QSize(-1, 32); }

 signals:
  void minimizeRequested();
  void maximizeRestoreRequested();
  void closeRequested();

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;

 private:
  QLabel* titleLabel_ = nullptr;
  QAbstractButton* minimizeButton_ = nullptr;
  QAbstractButton* maximizeButton_ = nullptr;
  QAbstractButton* closeButton_ = nullptr;
  bool maximized_ = false;
};
```

- [ ] **Step 4: 创建 `src/app/widgets/CaptionBar.cpp`**

```cpp
#include "app/widgets/CaptionBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QWindow>

#include "app/UiStyle.h"

namespace {

// 标题栏按钮：46x32，自绘 Fluent 图标（最小化/最大化/还原/关闭）。
class CaptionButton final : public QAbstractButton {
  Q_OBJECT
 public:
  enum class Icon { Minimize, Maximize, Restore, Close };

  explicit CaptionButton(Icon icon, QWidget* parent = nullptr)
      : QAbstractButton(parent), icon_(icon) {
    setFixedSize(46, 32);
    setMouseTracking(true);
  }

  void setIcon(Icon icon) {
    icon_ = icon;
    update();
  }

  void setForceHovered(bool hovered) {
    if (forceHovered_ == hovered) return;
    forceHovered_ = hovered;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const ThemeTokens& tokens = fluentLightTokens();
    const bool hovered = isDown() || forceHovered_ || underMouse();
    const bool close = icon_ == Icon::Close;

    if (hovered) {
      painter.fillRect(rect(), close ? tokens.danger : QColor(0, 0, 0, 15));
    }

    QColor iconColor = (hovered && close) ? QColor(Qt::white) : tokens.textPrimary;
    QPen pen(iconColor, 1.0);
    painter.setPen(pen);

    const QRectF iconRect = QRectF(0, 0, 10, 10);
    const QPointF c = rect().center();
    const QRectF r(c.x() - 5, c.y() - 5, 10, 10);
    switch (icon_) {
      case Icon::Minimize:
        painter.drawLine(QPointF(r.left(), c.y() + 0.5),
                         QPointF(r.right() + 1, c.y() + 0.5));
        break;
      case Icon::Maximize:
        painter.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
        break;
      case Icon::Restore: {
        // 后一个方框 + 前一个方框（错位重叠）
        painter.drawRect(QRectF(r.left() + 2.5, r.top() + 0.5, 7, 7));
        painter.fillRect(QRectF(r.left() + 0.5, r.top() + 2.5, 7, 7),
                         palette().window().color());
        painter.drawRect(QRectF(r.left() + 0.5, r.top() + 2.5, 7, 7));
        break;
      }
      case Icon::Close:
        painter.drawLine(r.topLeft(), r.bottomRight() + QPointF(1, 1));
        painter.drawLine(r.topRight() + QPointF(1, 0),
                         r.bottomLeft() + QPointF(0, 1));
        break;
    }
  }

 private:
  Icon icon_;
  bool forceHovered_ = false;
};

}  // namespace

CaptionBar::CaptionBar(const QString& title, QWidget* parent)
    : QWidget(parent) {
  setFixedHeight(32);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(12, 0, 0, 0);
  layout->setSpacing(0);

  titleLabel_ = new QLabel(title, this);
  titleLabel_->setObjectName(QStringLiteral("captionTitle"));
  layout->addWidget(titleLabel_);
  layout->addStretch(1);

  auto* minimize = new CaptionButton(CaptionButton::Icon::Minimize, this);
  minimize->setObjectName(QStringLiteral("captionMinimizeButton"));
  minimize->setToolTip(QStringLiteral("最小化"));
  auto* maximize = new CaptionButton(CaptionButton::Icon::Maximize, this);
  maximize->setObjectName(QStringLiteral("captionMaximizeButton"));
  maximize->setToolTip(QStringLiteral("最大化"));
  auto* close = new CaptionButton(CaptionButton::Icon::Close, this);
  close->setObjectName(QStringLiteral("captionCloseButton"));
  close->setToolTip(QStringLiteral("关闭"));

  minimizeButton_ = minimize;
  maximizeButton_ = maximize;
  closeButton_ = close;
  layout->addWidget(minimize);
  layout->addWidget(maximize);
  layout->addWidget(close);

  connect(minimize, &QAbstractButton::clicked, this, &CaptionBar::minimizeRequested);
  connect(maximize, &QAbstractButton::clicked, this,
          &CaptionBar::maximizeRestoreRequested);
  connect(close, &QAbstractButton::clicked, this, &CaptionBar::closeRequested);
}

void CaptionBar::setMaximized(bool maximized) {
  if (maximized_ == maximized) return;
  maximized_ = maximized;
  static_cast<CaptionButton*>(maximizeButton_)
      ->setIcon(maximized ? CaptionButton::Icon::Restore
                          : CaptionButton::Icon::Maximize);
  maximizeButton_->setToolTip(maximized ? QStringLiteral("还原")
                                        : QStringLiteral("最大化"));
}

void CaptionBar::setMaximizeButtonHovered(bool hovered) {
  static_cast<CaptionButton*>(maximizeButton_)->setForceHovered(hovered);
}

void CaptionBar::mousePressEvent(QMouseEvent* event) {
  // 空白区域拖动窗口；最大化时不拖动（避免误触，双击可还原）。
  if (event->button() == Qt::LeftButton && !maximized_ && window() &&
      window()->windowHandle()) {
    window()->windowHandle()->startSystemMove();
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void CaptionBar::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    emit maximizeRestoreRequested();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

#include "CaptionBar.moc"
```

注意：文件末尾的 `#include "CaptionBar.moc"` 是因为匿名命名空间里的 `CaptionButton` 带 Q_OBJECT——AUTOMOC 需要这行才能为其生成元对象代码。

- [ ] **Step 5: 跑测试确认通过 + 全量回归**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowProductShellTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R ProductShell --output-on-failure
cmake --build build/windows-vs2026-debug --config Debug
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

预期：全部通过。

- [ ] **Step 6: Commit**

```bash
git add src/app/widgets/CaptionBar.h src/app/widgets/CaptionBar.cpp tests/ProductShellTests.cpp CMakeLists.txt
git commit -m "功能：新增沉浸式标题栏 CaptionBar 组件

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 6: MainWindow 沉浸式集成（WM_NCCALCSIZE + 装配）

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`（buildUi 装配、nativeEvent、changeEvent）
- Test: `tests/MainWindowTests.cpp`

**Interfaces:**
- Consumes: Task 5 的 `CaptionBar`（构造 `CaptionBar(title, parent)`、三个信号、`setMaximized`、`setMaximizeButtonHovered`、按钮 objectName `"captionMaximizeButton"`）；Task 1/2 的窗口样式服务
- Produces: 无新接口（收尾集成）

**背景：** 用 WM_NCCALCSIZE 返回 0 去掉非客户区，保留 DWM 原生圆角/阴影/Snap/Mica；最大化时需按边框厚度内缩避免盖住任务栏；WM_NCHITTEST 提供四周边框调整区 + 最大化按钮 HTMAXBUTTON（Snap Layouts）。

- [ ] **Step 1: 写失败测试**

`tests/MainWindowTests.cpp` 顶部追加 `#include "app/widgets/CaptionBar.h"`，`private slots:` 追加 `void immersiveCaptionIsInstalled();`，实现：

```cpp
void MainWindowTests::immersiveCaptionIsInstalled() {
  const QString appName =
      QString("QtClickerMainWindowCaptionTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository));

  auto* caption = window.findChild<CaptionBar*>();
  QVERIFY(caption);
  // 无边框走 WM_NCCALCSIZE 方案，不得使用 FramelessWindowHint
  QVERIFY(!window.windowFlags().testFlag(Qt::FramelessWindowHint));
  QCOMPARE(window.windowTitle(), QString("ClickFlow"));
  QVERIFY(caption->findChild<QAbstractButton*>("captionMaximizeButton"));
  QVERIFY(caption->findChild<QAbstractButton*>("captionMinimizeButton"));
  QVERIFY(caption->findChild<QAbstractButton*>("captionCloseButton"));
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R MainWindow --output-on-failure
```

预期：失败（找不到 CaptionBar）。

- [ ] **Step 3: 修改 `src/app/MainWindow.h`**

在类声明中：

```cpp
// 前置声明区追加
class CaptionBar;

// private slots 保持不变；protected/private 区追加：
 protected:
  bool nativeEvent(const QByteArray& eventType, void* message,
                   qintptr* result) override;
  void changeEvent(QEvent* event) override;

// private 成员区追加（sidebar_ 等成员附近）
  CaptionBar* captionBar_ = nullptr;
```

- [ ] **Step 4: buildUi 装配 CaptionBar**

`src/app/MainWindow.cpp` 顶部追加 include：

```cpp
#include "app/widgets/CaptionBar.h"
#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>
#endif
```

`buildUi()` 开头重构布局（把 CaptionBar 置于侧边栏+内容区之上）：

```cpp
void MainWindow::buildUi() {
  setWindowTitle("ClickFlow");  // 提前设置，CaptionBar 读取同一标题

  auto* central = new QWidget(this);
  setCentralWidget(central);
  auto* outer = new QVBoxLayout(central);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  captionBar_ = new CaptionBar(windowTitle(), central);
  outer->addWidget(captionBar_);

  auto* shellContainer = new QWidget(central);
  auto* shell = new QHBoxLayout(shellContainer);
  shell->setContentsMargins(0, 0, 0, 0);
  shell->setSpacing(0);
  sidebar_ = new NavigationSidebar(shellContainer);
  shell->addWidget(sidebar_);

  auto* content = new QWidget(shellContainer);
  content->setObjectName("contentSurface");
  // ……其余构建代码保持不变……
  shell->addWidget(content, 1);
  outer->addWidget(shellContainer, 1);

  // 原有代码：setMinimumSize/resize/setStyleSheet/prepare（删除原来的 setWindowTitle 一行）
}
```

注意：原 `buildUi()` 末尾的 `setWindowTitle("ClickFlow");` 删除（已移到开头）。原 `auto* shell = new QHBoxLayout(central);` 改为挂在 `shellContainer` 上（如上）。

在构造函数的信号连接区（`connect(actionBar_, ...)` 附近）追加：

```cpp
  connect(captionBar_, &CaptionBar::minimizeRequested, this, &QWidget::showMinimized);
  connect(captionBar_, &CaptionBar::maximizeRestoreRequested, this, [this] {
    if (isMaximized()) showNormal();
    else showMaximized();
  });
  connect(captionBar_, &CaptionBar::closeRequested, this, &QWidget::close);
```

- [ ] **Step 5: 实现 nativeEvent 与 changeEvent**

`src/app/MainWindow.cpp` 追加：

```cpp
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message,
                             qintptr* result) {
#if defined(Q_OS_WIN)
  if (eventType == "windows_generic_MSG") {
    auto* msg = static_cast<MSG*>(message);
    switch (msg->message) {
      case WM_NCCALCSIZE: {
        if (msg->wParam == FALSE) break;  // 交给默认处理
        // 返回 0：去掉非客户区，内容铺满窗口；DWM 原生圆角/阴影/Snap 保留。
        if (IsZoomed(msg->hwnd)) {
          // 最大化时按边框厚度内缩，避免盖住任务栏、边缘被裁剪。
          auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
          const UINT dpi = GetDpiForWindow(msg->hwnd);
          const int frameX = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                             GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
          const int frameY = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                             GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
          params->rgrc[0].left += frameX;
          params->rgrc[0].right -= frameX;
          params->rgrc[0].top += frameY;
          params->rgrc[0].bottom -= frameY;
        }
        *result = 0;
        return true;
      }
      case WM_NCHITTEST: {
        const POINT cursor{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        // 最大化按钮命中：返回 HTMAXBUTTON 让 Win11 Snap Layouts 面板生效。
        if (captionBar_) {
          if (auto* maxButton =
                  captionBar_->findChild<QAbstractButton*>("captionMaximizeButton")) {
            const QPoint local =
                maxButton->mapFromGlobal(QPoint(cursor.x, cursor.y));
            if (maxButton->rect().contains(local)) {
              *result = HTMAXBUTTON;
              return true;
            }
          }
        }
        if (IsZoomed(msg->hwnd)) break;  // 最大化不提供边框调整
        RECT windowRect{};
        GetWindowRect(msg->hwnd, &windowRect);
        const UINT dpi = GetDpiForWindow(msg->hwnd);
        const int border = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                           GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        const int x = cursor.x - windowRect.left;
        const int y = cursor.y - windowRect.top;
        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        const bool onLeft = x < border;
        const bool onRight = x >= width - border;
        const bool onTop = y < border;
        const bool onBottom = y >= height - border;
        if (onTop && onLeft) { *result = HTTOPLEFT; return true; }
        if (onTop && onRight) { *result = HTTOPRIGHT; return true; }
        if (onBottom && onLeft) { *result = HTBOTTOMLEFT; return true; }
        if (onBottom && onRight) { *result = HTBOTTOMRIGHT; return true; }
        if (onLeft) { *result = HTLEFT; return true; }
        if (onRight) { *result = HTRIGHT; return true; }
        if (onTop) { *result = HTTOP; return true; }
        if (onBottom) { *result = HTBOTTOM; return true; }
        break;
      }
      case WM_NCMOUSEMOVE: {
        // HTMAXBUTTON 区域收不到普通 hover 事件，手动同步按钮悬停态。
        if (captionBar_) {
          if (auto* maxButton =
                  captionBar_->findChild<QAbstractButton*>("captionMaximizeButton")) {
            const POINT cursor{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            const QPoint local =
                maxButton->mapFromGlobal(QPoint(cursor.x, cursor.y));
            captionBar_->setMaximizeButtonHovered(
                maxButton->rect().contains(local));
          }
        }
        break;
      }
      default:
        break;
    }
  }
#endif
  return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
  if (event->type() == QEvent::WindowStateChange && captionBar_) {
    captionBar_->setMaximized(isMaximized());
  }
}
```

- [ ] **Step 6: 跑测试确认通过 + 全量回归**

```bash
cmake --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R MainWindow --output-on-failure
cmake --build build/windows-vs2026-debug --config Debug
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

预期：全部通过。

- [ ] **Step 7: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp tests/MainWindowTests.cpp
git commit -m "功能：主窗口沉浸式标题栏与无边框集成

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

### Task 7: 手动验证与右缘溢出收尾

**Files:**
- 可能修改：`src/app/pages/*.cpp`（仅在发现具体溢出根因时）
- Test: 若修复了布局问题，补对应断言到 `tests/ClickFlowPageTests.cpp` 或 `tests/MainWindowTests.cpp`

**Interfaces:**
- Consumes: Task 1-6 的全部成果
- Produces: 无（验证任务；若产出修复则接口不变）

**背景：** 右缘 "te" 残留最可能是 Mica 缺失时透明区域直接透出后方窗口（黑桌面 + 后方窗口文字），即缺陷 C 是缺陷 A 的共生症状。Task 1-2 修复后大概率消失。本任务先做全量回归，再启动应用按清单人工验证；若残留仍在，按下述排查路径定位。

- [ ] **Step 1: 全量构建与测试**

```bash
cmake --build build/windows-vs2026-debug --config Debug
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

预期：全部通过（共 9 个测试程序）。

- [ ] **Step 2: 启动应用，人工验证**

```bash
build/windows-vs2026-debug/Debug/ClickFlow.exe
```

对照 spec §7 清单逐项检查：

1. Mica 云母正常透出（内容区不再是纯黑，能看到云母质感）
2. 右边缘无渲染残留；逐页切换（连点设置/键鼠录制/热键/预设与关于）确认无横向溢出
3. 标题栏：空白区拖动、双击最大化/还原、三个按钮功能正常；悬停最大化按钮出现 Snap Layouts 面板
4. 勾选/取消「保持窗口置顶」后，Mica 与标题栏表现不退化
5. 最大化 → 还原、拖到屏幕边缘 Snap → 还原，布局正常、标题栏状态正确切换
6. 多显示器/不同 DPI 间拖动窗口，标题栏与控件尺寸无错乱
7. （可选）系统设置关闭「透明效果」后重启应用：回退为不透明 `#F3F3F3` 浅色，界面完整可用
8. 既有功能冒烟：启动/停止连点、录制页与预设页可正常打开

- [ ] **Step 3: 若右缘残留仍存在，按以下顺序排查**

1. 确认残留是否随 Mica 修复消失——若 Mica 已正常而残留仍在，则不是透出后方窗口，继续往下查。
2. `grep -n "setFixedWidth\|setMinimumWidth\|setFixedSize\|setMinimumSize" src/app/pages/*.cpp src/app/widgets/*.cpp`——找出是否有子控件最小宽度撑爆视口；发现后改为 `QSizePolicy` 弹性约束。
3. 检查 `QStackedWidget` 内非当前页是否被意外 `show()`（正常情况下 `SmoothScrollArea` + `setWidgetResizable(true)` 不会溢出）。
4. 检查是否为半透明窗口重绘残留：切换页面、缩放窗口各一次后残留是否消失；若消失属合成器刷新问题，在 `pages_` 切换处（MainWindow.cpp 的 `pageSelected` lambda）加 `pages_->currentWidget()->update();`。
5. 每处修复补一条对应断言（如页面 `minimumSizeHint().width()` 不超过阈值），提交信息用 `修复：…`。

- [ ] **Step 4: 全部验证通过后，更新计划勾选状态并向用户汇报验收结果**

无需提交（除非 Step 3 产生了修复）。

---

## Self-Review 记录

- **Spec 覆盖**：§1.1 Mica→Task 1/2；§1.2 右缘→Task 7（排查路径具体化）；§2 令牌+模板 QSS→Task 3；§3 标题栏→Task 5/6；§4 视觉→Task 3/4；§5 改动清单→全覆盖；§6 兜底→Task 1（透明效果回退）+ Task 6（NCCALCSIZE 保留系统菜单）；§7 验证→Task 7。
- **占位符**：无 TBD/TODO；每个代码步骤均含完整代码。
- **类型一致性**：`fluentLightTokens()`/`ThemeTokens` 字段名在 Task 3/4/5 一致；`CaptionBar` 接口在 Task 5 Produces 与 Task 6 Consumes 一致（`setMaximizeButtonHovered`、按钮 objectName）；`WindowStyleService` 三方法接口在 Task 1 Produces 与 Task 2 假实现一致。
