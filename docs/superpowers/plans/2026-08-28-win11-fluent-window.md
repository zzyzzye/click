# Win11 Fluent 窗口外壳（Mica 透出方案）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 ClickFlow 主窗口添加 Win11 原生 Fluent 外壳（圆角 + Mica 云母 + 暗色标题栏），Mica 从卡片间隙透出，卡片与控件外观保持不变。

**Architecture:** 新增平台接口 `WindowStyleService`（仿现有 `ClickBackend`/`HotkeyService` 工厂模式）。Windows 实现调 DWM 属性；macOS/测试为 no-op。`MainWindow::showEvent` 在每次 show 后重新应用 DWM 属性（覆盖置顶切换重建窗口的场景）。`clickFlowStyleSheet(bool translucent)` 提供 QSS 透明变体。

**Tech Stack:** Qt 6.8.3 Widgets、C++20、Windows DWM API（`dwmapi`，已链接）、MSVC 2022。

## Global Constraints

- C++20、CMake 3.24+、Qt 6.8.3（`Widgets`/`Network`/`Test`）。
- Windows 平台代码置于 `src/platform/windows/`，遵循现有文件命名（`WindowsXxx.cpp/.h`）。
- `dwmapi` 已链接到主程序和 `QtClickerMainWindowTests`，**不得新增链接依赖**。
- macOS 提供与 Windows 行为一致的 no-op（`usesBackdrop()` 返回 false）。
- 既有测试断言必须保持通过：`usesClickFlowControlChrome`（QSS 含 `QComboBox::down-arrow`、`#sidebarNavigation { background: transparent`）、控件高度断言（普通 40、主操作 44）。
- 不删除现有 QSS 控件样式；只新增透明变体。
- commit message 末尾附 `Co-Authored-By: Claude <noreply@anthropic.com>`。

---

## File Structure

| 文件 | 责任 | 状态 |
|---|---|---|
| `src/platform/WindowStyleService.h` | 平台无关接口 + 工厂声明 | 新建 |
| `src/platform/windows/WindowsWindowStyle.h` | Windows 实现声明 | 新建 |
| `src/platform/windows/WindowsWindowStyle.cpp` | DWM 调用 + Win11 检测 + 注册表主题读取 | 新建 |
| `src/platform/windows/PlatformServicesWindows.cpp` | 提供工厂 `createWindowStyleService()` | 修改 |
| `src/platform/macos/PlatformServicesMac.mm` | macOS no-op 工厂 | 修改 |
| `src/app/UiStyle.h` / `UiStyle.cpp` | `clickFlowStyleSheet(bool translucent)` 透明变体 | 修改 |
| `src/app/MainWindow.h` / `MainWindow.cpp` | 注入 `WindowStyleService`、`showEvent`、`prepare`/`apply` | 修改 |
| `tests/MainWindowTests.cpp` | `isWindows11OrLater` 单测 + QSS 变体断言 | 修改 |
| `CMakeLists.txt` | 将新文件加入 `PLATFORM_SOURCES` 与测试源列表 | 修改 |

---

### Task 1: WindowStyleService 接口与 no-op 工厂

建立平台无关接口和 macOS/no-op 实现，让后续 Task 有可注入的桩。

**Files:**
- Create: `src/platform/WindowStyleService.h`
- Modify: `src/platform/macos/PlatformServicesMac.mm`
- Test: `tests/MainWindowTests.cpp`（仅验证头文件可包含、no-op 工厂返回非空）

**Interfaces:**
- Produces: `WindowStyleService` 接口（`prepare`/`apply`/`usesBackdrop`/`prefersDarkTheme`）、工厂 `createWindowStyleService()`。后续 Task 2 的 Windows 实现、Task 4 的 MainWindow 注入都依赖此签名。

- [ ] **Step 1: 编写接口头文件**

创建 `src/platform/WindowStyleService.h`：

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
  virtual bool prefersDarkTheme() const = 0;
};

std::unique_ptr<WindowStyleService> createWindowStyleService();
```

- [ ] **Step 2: 在 macOS 工厂文件提供 no-op**

在 `src/platform/macos/PlatformServicesMac.mm` 顶部 include 区追加（文件已存在，包含其他 `createXxx` 工厂函数）：

```cpp
#include "platform/WindowStyleService.h"
```

在该文件底部（其他工厂函数之后）追加 no-op 实现：

```cpp
namespace {
class NullWindowStyleService final : public WindowStyleService {
 public:
  void prepare(QWidget*) override {}
  void apply(QWidget*) override {}
  bool usesBackdrop() const override { return false; }
  bool prefersDarkTheme() const override { return false; }
};
}

std::unique_ptr<WindowStyleService> createWindowStyleService() {
  return std::make_unique<NullWindowStyleService>();
}
```

> Windows 工厂在 Task 2 的 `PlatformServicesWindows.cpp` 中提供。本 Task 只需保证 macOS 路径编译通过；Windows 路径在 Task 2 前 `createWindowStyleService()` 在 Windows 上未定义，Task 2 会补上。

- [ ] **Step 3: 验证 macOS 路径编译**

由于本机为 Windows，无法直接编译 macOS。改用确认 Windows 路径暂未引用该工厂（主程序入口在 Task 4 才接入）。跳过编译，进入 Task 2 一并在 Windows 上验证。

- [ ] **Step 4: Commit**

```bash
git add src/platform/WindowStyleService.h src/platform/macos/PlatformServicesMac.mm
git commit -m "$(cat <<'EOF'
feat: add WindowStyleService platform interface

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Windows DWM 实现与 Win11 检测

Windows 平台 `WindowsWindowStyle`，调 DWM 圆角/Mica/暗色标题栏，纯函数 `isWindows11OrLater` 可单测。

**Files:**
- Create: `src/platform/windows/WindowsWindowStyle.h`
- Create: `src/platform/windows/WindowsWindowStyle.cpp`
- Modify: `src/platform/windows/PlatformServicesWindows.cpp`
- Test: `tests/WindowsWindowStyleTests.cpp`（新建）、`CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的 `WindowStyleService` 接口。
- Produces: `isWindows11OrLater(quint32 build)` 纯函数（`build >= 22000`）、`WindowsWindowStyle` 类、Windows 版 `createWindowStyleService()`。Task 4 注入时用工厂；单测直接测 `isWindows11OrLater`。

- [ ] **Step 1: 编写 `isWindows11OrLater` 的失败测试**

创建 `tests/WindowsWindowStyleTests.cpp`：

```cpp
#include <QTest>

#include "platform/windows/WindowsWindowStyle.h"

class WindowsWindowStyleTests : public QObject {
  Q_OBJECT

 private slots:
  void windows11DetectedAtBuild22000();
  void windows10RejectedAtBuild19045();
  void boundaryBuild21999IsRejected();
};

void WindowsWindowStyleTests::windows11DetectedAtBuild22000() {
  QVERIFY(isWindows11OrLater(22000));
}

void WindowsWindowStyleTests::windows10RejectedAtBuild19045() {
  QVERIFY(!isWindows11OrLater(19045));
}

void WindowsWindowStyleTests::boundaryBuild21999IsRejected() {
  QVERIFY(!isWindows11OrLater(21999));
}

QTEST_MAIN(WindowsWindowStyleTests)
#include "WindowsWindowStyleTests.moc"
```

- [ ] **Step 2: 在 CMakeLists 注册该测试可编译（但预期失败）**

在 `CMakeLists.txt` 的 `if(WIN32)` 测试块内（`QtClickerWindowsHotkeyTests` 之后）追加：

```cmake
  add_executable(WindowsWindowStyleTests
    tests/WindowsWindowStyleTests.cpp
    src/platform/windows/WindowsWindowStyle.cpp
    src/platform/windows/WindowsWindowStyle.h
  )
  target_include_directories(WindowsWindowStyleTests PRIVATE src)
  target_link_libraries(WindowsWindowStyleTests PRIVATE
    Qt6::Core
    Qt6::Test
    dwmapi
  )
  add_test(NAME WindowsWindowStyle COMMAND WindowsWindowStyleTests)
  set_tests_properties(WindowsWindowStyle PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
```

- [ ] **Step 3: 运行测试验证编译失败**

Run: `cmake --build build/windows-msvc-debug --config Debug --target WindowsWindowStyleTests`
Expected: 编译失败，`WindowsWindowStyle.h` 不存在 / `isWindows11OrLater` 未定义。

- [ ] **Step 4: 编写 `WindowsWindowStyle.h`**

创建 `src/platform/windows/WindowsWindowStyle.h`：

```cpp
#pragma once

#include <memory>

#include "platform/WindowStyleService.h"

class QWidget;

// 纯函数，便于单元测试。传入注册表读到的 CurrentBuildNumber。
bool isWindows11OrLater(unsigned int buildNumber);

class WindowsWindowStyle final : public WindowStyleService {
 public:
  void prepare(QWidget* window) override;
  void apply(QWidget* window) override;
  bool usesBackdrop() const override;
  bool prefersDarkTheme() const override;
};
```

- [ ] **Step 5: 编写 `WindowsWindowStyle.cpp`**

创建 `src/platform/windows/WindowsWindowStyle.cpp`：

```cpp
#include "platform/windows/WindowsWindowStyle.h"

#include <QGuiApplication>
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

constexpr int kDwmwcpRound = 2;          // DWMWCP_ROUND
constexpr int kDwmsbtMainWindow = 2;     // DWMSBT_MAINWINDOW（Mica）

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

bool readAppsUseLightTheme() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0, KEY_READ, &key) != ERROR_SUCCESS) {
    return true;  // 默认亮色
  }
  DWORD value = 1;
  DWORD size = sizeof(value);
  RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                   reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(key);
  return value != 0;
}

HWND widgetHandle(const QWidget* widget) {
  return reinterpret_cast<HWND>(widget->winId());
}

}  // namespace

bool isWindows11OrLater(unsigned int buildNumber) {
  return buildNumber >= 22000;
}

void WindowsWindowStyle::prepare(QWidget* window) {
  if (!window) return;
  if (isWindows11OrLater(currentBuildNumber())) {
    window->setAttribute(Qt::WA_TranslucentBackground);
  }
}

void WindowsWindowStyle::apply(QWidget* window) {
  if (!window) return;
  if (!isWindows11OrLater(currentBuildNumber())) return;

  HWND handle = widgetHandle(window);
  if (!handle) return;

  const int corner = kDwmwcpRound;
  DwmSetWindowAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                        sizeof(corner));

  const int backdrop = kDwmsbtMainWindow;
  DwmSetWindowAttribute(handle, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop,
                        sizeof(backdrop));

  const BOOL dark = readAppsUseLightTheme() ? FALSE : TRUE;
  DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                        sizeof(dark));
}

bool WindowsWindowStyle::usesBackdrop() const {
  return isWindows11OrLater(currentBuildNumber());
}

bool WindowsWindowStyle::prefersDarkTheme() const {
  return !readAppsUseLightTheme();
}
```

- [ ] **Step 6: 在 Windows 工厂文件提供 `createWindowStyleService`**

修改 `src/platform/windows/PlatformServicesWindows.cpp`，在 include 区追加：

```cpp
#include "platform/windows/WindowsWindowStyle.h"
```

在文件底部（其他工厂函数之后）追加：

```cpp
std::unique_ptr<WindowStyleService> createWindowStyleService() {
  return std::make_unique<WindowsWindowStyle>();
}
```

- [ ] **Step 7: 运行测试验证通过**

Run: `cmake --build build/windows-msvc-debug --config Debug --target WindowsWindowStyleTests && ctest --test-dir build/windows-msvc-debug -C Debug -R WindowsWindowStyle --output-on-failure`
Expected: 3 个测试全 PASS。

- [ ] **Step 8: Commit**

```bash
git add src/platform/windows/WindowsWindowStyle.h src/platform/windows/WindowsWindowStyle.cpp src/platform/windows/PlatformServicesWindows.cpp tests/WindowsWindowStyleTests.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: implement Windows DWM Mica window style service

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: QSS 透明变体

为 `clickFlowStyleSheet` 增加布尔参数，Win11 透出时让窗口外壳与间隙背景变透明，卡片保持不透明。

**Files:**
- Modify: `src/app/UiStyle.h`
- Modify: `src/app/UiStyle.cpp`
- Test: `tests/MainWindowTests.cpp`（`usesClickFlowControlChrome` 已存在，补一个透明变体断言）

**Interfaces:**
- Produces: `clickFlowStyleSheet(bool translucent = false)`。Task 4 的 MainWindow 用 `clickFlowStyleSheet(styleService_->usesBackdrop())` 调用。

- [ ] **Step 1: 修改 `UiStyle.h` 签名**

修改 `src/app/UiStyle.h`：

```cpp
#pragma once

#include <QString>

QString clickFlowStyleSheet(bool translucent = false);
```

- [ ] **Step 2: 修改 `UiStyle.cpp` 支持透明变体**

修改 `src/app/UiStyle.cpp` 的 `clickFlowStyleSheet` 函数。在函数开头接收参数并据其选择首行背景与透明声明。将现有 `return QStringLiteral(R"( ... )")` 改为根据 `translucent` 拼接。实现如下（替换整个函数体）：

```cpp
#include "app/UiStyle.h"

QString clickFlowStyleSheet(bool translucent) {
  const char* surfaceBackground =
      translucent ? "transparent" : "#f4f5f7";
  const char* contentBackground = translucent ? "transparent" : "#f4f5f7";

  return QStringLiteral(R"(
    QMainWindow, #contentSurface { background: %1; color: #18202b; }
    #contentPages { background: %2; }
    QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
    #navigationSidebar { background: #e9ecf1; border-right: 1px solid #d4d9e1; }
    #productName { font-size: 22px; font-weight: 700; color: #14213d; }
    #productVersion { color: #6b7280; }
    #sidebarNavigation {
      background: transparent; border: none; outline: none;
    }
    #sidebarNavigation::item { border-radius: 8px; padding-left: 12px; }
    #sidebarNavigation::item:selected { background: #2563eb; color: white; }
    #settingsCard, #statusStrip, #actionBar {
      background: white; border: 1px solid #dfe3e8; border-radius: 10px;
    }
    #cardTitle { font-size: 16px; font-weight: 650; }
    QPushButton {
      min-height: 38px; max-height: 38px;
      background: white; color: #273244;
      border: 1px solid #cfd5dd; border-radius: 8px;
      padding: 0 16px; font-weight: 550;
    }
    QPushButton:hover {
      background: #f7f9fc; border-color: #9eabc0;
    }
    QPushButton:pressed {
      background: #edf1f7; border-color: #7f8da3;
    }
    QPushButton:focus { border-color: #2563eb; }
    QPushButton:disabled {
      color: #9aa3b2; background: #f5f6f8; border-color: #e1e5ea;
    }
    QPushButton#startStopButton,
    QPushButton#macroRecordButton,
    QPushButton#macroPlayButton {
      min-height: 44px; max-height: 44px;
      color: white; border: 0; border-radius: 9px;
      padding: 0 22px; font-weight: 650;
    }
    QPushButton#startStopButton,
    QPushButton#macroRecordButton { background: #2563eb; }
    QPushButton#macroPlayButton { background: #173b66; }
    QPushButton#startStopButton:hover,
    QPushButton#macroRecordButton:hover { background: #1d4ed8; }
    QPushButton#macroPlayButton:hover { background: #102f55; }
    QPushButton#startStopButton:pressed,
    QPushButton#macroRecordButton:pressed { background: #1e40af; }
    QPushButton#macroPlayButton:pressed { background: #0b2647; }
    QPushButton#startStopButton:disabled,
    QPushButton#macroRecordButton:disabled,
    QPushButton#macroPlayButton:disabled {
      color: #cbd5e1; background: #94a3b8;
    }
    QPushButton#startStopButton[running="true"] { background: #dc2626; }
    QPushButton#startStopButton[running="true"]:hover { background: #b91c1c; }
    QPushButton#startStopButton[running="true"]:pressed { background: #991b1b; }
    QComboBox, QSpinBox, QKeySequenceEdit {
      min-height: 38px; max-height: 38px;
      border: 1px solid #cfd5dd; border-radius: 8px;
      background: white; padding: 0 34px 0 10px;
    }
    QComboBox:hover, QSpinBox:hover, QKeySequenceEdit:hover {
      border-color: #9eabc0;
    }
    QComboBox:focus, QSpinBox:focus, QKeySequenceEdit:focus {
      border: 1px solid #2563eb;
    }
    QComboBox::drop-down {
      subcontrol-origin: padding; subcontrol-position: top right;
      width: 30px; margin: 3px; border: none; border-radius: 5px;
    }
    QComboBox::drop-down:hover { background: #edf3ff; }
    QComboBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.svg);
      width: 12px; height: 8px;
    }
    QSpinBox { padding-right: 32px; }
    QSpinBox::up-button, QSpinBox::down-button {
      subcontrol-origin: border; width: 28px;
      border: none; background: transparent;
    }
    QSpinBox::up-button {
      subcontrol-position: top right; margin: 3px 3px 0 0;
      border-top-left-radius: 5px; border-top-right-radius: 5px;
    }
    QSpinBox::down-button {
      subcontrol-position: bottom right; margin: 0 3px 3px 0;
      border-bottom-left-radius: 5px; border-bottom-right-radius: 5px;
    }
    QSpinBox::up-button:hover, QSpinBox::down-button:hover {
      background: #edf3ff;
    }
    QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
      background: #dce8ff;
    }
    QSpinBox::up-arrow {
      image: url(:/clickflow/icons/chevron-up.svg);
      width: 10px; height: 6px;
    }
    QSpinBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.svg);
      width: 10px; height: 6px;
    }
    QComboBox:disabled, QSpinBox:disabled, QKeySequenceEdit:disabled {
      color: #8a94a3; background: #f5f6f8;
    }
  )")
      .arg(QString::fromLatin1(surfaceBackground),
           QString::fromLatin1(contentBackground));
}
```

> 说明：`translucent=false` 时 `%1`/`%2` 均为 `#f4f5f7`，与现状逐字一致；`translucent=true` 时窗口外壳与 `#contentPages` 透明，卡片 `#settingsCard`/`#statusStrip`/`#actionBar`/`#navigationSidebar` 仍不透明，Mica 从间隙透出。

- [ ] **Step 3: 为透明变体补测试**

在 `tests/MainWindowTests.cpp` 的 `usesClickFlowControlChrome` 测试旁新增一个测试槽（在 private slots 区域内，紧随其后）：

```cpp
void transparentStyleSheetKeepsCardsOpaque() {
  const QString style = clickFlowStyleSheet(true);
  const QString compact = style.simplified();
  // 外壳与内容页透明
  QVERIFY(compact.contains("QMainWindow, #contentSurface { background: transparent"));
  QVERIFY(compact.contains("#contentPages { background: transparent"));
  // 卡片保持不透明白底
  QVERIFY(compact.contains(
      "#settingsCard, #statusStrip, #actionBar { background: white"));
  // 既有断言要素仍在
  QVERIFY(style.contains("QComboBox::down-arrow"));
}
```

并在该测试文件顶部 include 区确保有（通常已有）：

```cpp
#include "app/UiStyle.h"
```

- [ ] **Step 4: 运行测试验证通过**

Run: `cmake --build build/windows-msvc-debug --config Debug --target QtClickerMainWindowTests && ctest --test-dir build/windows-msvc-debug -C Debug -R MainWindow --output-on-failure`
Expected: `usesClickFlowControlChrome`、`transparentStyleSheetKeepsCardsOpaque` 均通过；其余既有用例无回归。

- [ ] **Step 5: 同时跑页面测试确认无回归**

Run: `ctest --test-dir build/windows-msvc-debug -C Debug -R ClickFlowPages --output-on-failure`
Expected: PASS（控件高度断言不受 `translucent` 默认值影响）。

- [ ] **Step 6: Commit**

```bash
git add src/app/UiStyle.h src/app/UiStyle.cpp tests/MainWindowTests.cpp
git commit -m "$(cat <<'EOF'
feat: add translucent QSS variant for Mica backdrop

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: MainWindow 接入 WindowStyleService

将 `WindowStyleService` 注入 MainWindow，`prepare` 在 `buildUi`、`apply` 在 `showEvent`，QSS 变体按 `usesBackdrop()` 选择。

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `CMakeLists.txt`（主程序 `PLATFORM_SOURCES` 与 `QtClickerMainWindowTests` 源列表加入新文件）

**Interfaces:**
- Consumes: Task 1 接口、Task 2 工厂、Task 3 QSS 变体。
- Produces: MainWindow 显示 Win11 外壳效果。

- [ ] **Step 1: 修改 `MainWindow.h`**

在 include 区追加：

```cpp
#include "platform/WindowStyleService.h"
```

在成员变量区（`lastHotkeyRegistrationError_;` 之后）追加：

```cpp
  std::unique_ptr<WindowStyleService> windowStyle_;
```

在 private 函数区（`void buildUi();` 附近）追加声明：

```cpp
  void showEvent(QShowEvent* event) override;
```

- [ ] **Step 2: 修改 `MainWindow.cpp` 构造链注入**

在文件顶部 include 区追加：

```cpp
#include "app/UiStyle.h"  // 已存在，确认在
```

（`UiStyle.h` 已 include，无需重复；仅确认。）

修改无参构造（第 88 行附近），在转发参数末尾、`parent` 前插入 `createWindowStyleService()`：

```cpp
MainWindow::MainWindow(QWidget* parent)
    : MainWindow(createClickBackend(), createHotkeyService(),
                 std::make_unique<SettingsRepository>(),
                 createMacroPlatformServices(),
                 std::make_unique<MacroRepository>(), {}, {},
                 createWindowStyleService(), parent) {}
```

修改 2 参测试构造（第 94 行附近），转发时补 `{}`：

```cpp
MainWindow::MainWindow(std::unique_ptr<ClickBackend> backend,
                       std::unique_ptr<HotkeyService> hotkeyService,
                       std::unique_ptr<SettingsRepository> settingsRepository,
                       QWidget* parent)
    : MainWindow(std::move(backend), std::move(hotkeyService),
                 std::move(settingsRepository), MacroPlatformServices{}, nullptr,
                 {}, {}, {}, parent) {}
```

修改大构造签名与初始化列表（第 102 行附近），新增参数并初始化成员：

签名末尾 `MacroNameProvider macroNameProvider, QWidget* parent` 改为：

```cpp
             MacroNameProvider macroNameProvider,
             std::unique_ptr<WindowStyleService> windowStyle,
             QWidget* parent = nullptr);
```

初始化列表中 `macroNameProvider_(std::move(macroNameProvider))` 之后、`{` 之前加入：

```cpp
      windowStyle_(std::move(windowStyle)),
```

并在构造体开头（`if (!safetyConfirmation_)` 之前）补默认值：

```cpp
  if (!windowStyle_) windowStyle_ = std::make_unique<NullWindowStyleService>();
```

> 注意：`NullWindowStyleService` 在 macOS 工厂文件匿名命名空间内定义，主程序无法直接 `make_unique`。改用更稳妥方式——见 Step 3 提供一个公开的工厂兜底。因此本步改为：构造体开头不引用 `NullWindowStyleService`，而是保证调用方总传非空（无参构造已传 `createWindowStyleService()`，2 参构造传 `{}` 会得到 `nullptr`）。需为 `{}` 路径提供兜底。

**修正 Step 2 的兜底实现**：构造体开头改为：

```cpp
  if (!windowStyle_) {
    windowStyle_ = createWindowStyleService();
  }
```

（`createWindowStyleService()` 在所有平台都有定义：Windows 返回 DWM 实现，macOS 返回 no-op。2 参测试构造传 `{}` → null → 走兜底，得到平台 no-op，offscreen 测试安全。）

- [ ] **Step 3: 在 `buildUi` 调 `prepare` 与 QSS 变体**

修改 `MainWindow::buildUi`（第 262 行附近）。在 `setStyleSheet(clickFlowStyleSheet());` 这行（第 311 行）改为：

```cpp
  setStyleSheet(clickFlowStyleSheet(windowStyle_->usesBackdrop()));
```

并在 `buildUi` 末尾（`setStyleSheet` 之后，函数 `}` 之前）追加：

```cpp
  windowStyle_->prepare(this);
```

> 顺序：先 `setStyleSheet`（QSS 变体由 `usesBackdrop()` 决定），再 `prepare`（设 `WA_TranslucentBackground`，仅 Win11）。两者都在首次 show 之前。

- [ ] **Step 4: 实现 `showEvent`**

在 `MainWindow.cpp` 中（`buildUi` 函数之后或 `applyWindowOnTop` 附近）新增：

```cpp
void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  windowStyle_->apply(this);
}
```

- [ ] **Step 5: 在 CMakeLists 主程序加入新文件**

在 `CMakeLists.txt` 的 `elseif(WIN32)` 块 `PLATFORM_SOURCES` 列表（第 106 行附近，`src/platform/windows/PlatformServicesWindows.cpp` 之后）追加：

```cmake
    src/platform/windows/WindowsWindowStyle.cpp
```

在对应 `APP_HEADERS` 或平台头列表不必追加（头文件随 cpp 自动发现，但为一致可在 Windows 头列表末尾加 `src/platform/windows/WindowsWindowStyle.h`）。

- [ ] **Step 6: 在 CMakeLists MainWindow 测试加入新文件**

`QtClickerMainWindowTests` 源列表（第 378 行附近，Windows 分支内）末尾的 `src/platform/windows/PlatformServicesWindows.cpp` 之后追加：

```cmake
    src/platform/windows/WindowsWindowStyle.cpp
    src/platform/windows/WindowsWindowStyle.h
```

macOS 分支的 `QtClickerMainWindowTests`（第 433 行附近）使用 `${PLATFORM_SOURCES}`，自动包含（macOS 无新文件，需确认 `PlatformServicesMac.mm` 已加入 `PLATFORM_SOURCES`——见 Task 1 已修改该文件）。检查 macOS 分支是否需要显式加 `src/platform/WindowStyleService.h`：该头为公共接口，已被 `MainWindow.h` include，无需单独加入源列表。

- [ ] **Step 7: 编译并运行全部测试**

Run: `cmake --build build/windows-msvc-debug --config Debug --parallel && ctest --test-dir build/windows-msvc-debug -C Debug --output-on-failure`
Expected: 全部测试通过，无回归（含 `MainWindow`、`ClickFlowPages`、`ProductShell`、`WindowsWindowStyle`、`WindowsMacro`、`WindowsClickBackend`、`WindowsHotkeyMapping`、`AppIdentity`、`MacroTypes`、`MacroController`）。

- [ ] **Step 8: 手动目视验证 Win11 效果**

Run（在真实桌面，非 offscreen）: 启动 `build/windows-msvc-debug/Debug/ClickFlow.exe`

目视确认：
1. 窗口四角为圆角；
2. 卡片间隙透出 Mica 云母背景（非纯灰）；
3. 卡片、按钮、输入框外观与改造前一致；
4. 勾选「保持窗口置顶」后取消，窗口圆角/Mica 仍在（验证 `showEvent` 重新应用）。

若 offscreen 下 `usesBackdrop()` 在 Win11 开发机返回 true 但无实际渲染，测试仍应全绿（DWM 调用在无桌面会话下失败被忽略，QSS 透明变体不影响断言）。

- [ ] **Step 9: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: apply Win11 Mica backdrop to main window

Co-Authored-By: Claude <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review 记录

- **Spec 覆盖**：接口与工厂（Task 1）、Windows DWM 实现 + Win11 检测 + 注册表主题（Task 2）、QSS 透明变体（Task 3）、MainWindow 注入 + showEvent + 置顶重建覆盖（Task 4）、CMake 链接（无需新增，已确认 dwmapi 已链接）、测试（Task 2 单测 + Task 3 变体断言 + Task 4 全量回归）。暗色模式留接口由 `prefersDarkTheme()` 提供，内容 QSS 暂固定亮色，符合 spec 取舍。
- **占位符扫描**：无 TBD/TODO，每个代码步骤均含完整代码。
- **类型一致性**：`isWindows11OrLater(unsigned int)` 在 Task 2 头、cpp、测试一致；`clickFlowStyleSheet(bool)` 在 Task 3 头、cpp、测试及 Task 4 调用一致；`WindowStyleService` 四方法签名在 Task 1/2/4 一致；构造链参数顺序经核对。
- **风险点**：Step 2 兜底改用 `createWindowStyleService()` 而非直接构造 no-op 类，避免跨平台可见性问题，已修正。
