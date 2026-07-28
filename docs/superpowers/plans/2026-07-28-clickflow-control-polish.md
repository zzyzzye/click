# ClickFlow Control Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the dated Qt combo-box and spin-box buttons with ClickFlow-specific soft chevrons and remove the large sidebar navigation frame.

**Architecture:** Package three small SVG chevrons in a Qt resource collection and reference them from the existing application-level style sheet. Keep all widget behavior and data flow unchanged; tests verify resource availability and the style contract before Windows visual inspection.

**Tech Stack:** C++20, Qt 6 Widgets, Qt Resource System, QSS, Qt Test, CMake 3.24+

## Global Constraints

- Keep the existing ClickFlow 0.2.0 page structure and business behavior unchanged.
- Use one ClickFlow control style on Windows and macOS.
- Preserve visible `QSpinBox` up/down controls.
- Remove the sidebar navigation list frame while retaining the selected blue rounded row.
- Do not run packaging scripts and do not modify `dist/`.
- Preserve the untracked `.vscode/launch.json`.

---

### Task 1: Add reusable ClickFlow chevron resources

**Files:**
- Create: `src/app/resources/chevron-down.svg`
- Create: `src/app/resources/chevron-up.svg`
- Create: `src/app/resources/ClickFlowResources.qrc`
- Modify: `CMakeLists.txt`
- Test: `tests/MainWindowTests.cpp`

**Interfaces:**
- Consumes: Qt Resource System support already enabled by `CMAKE_AUTORCC`.
- Produces: `:/clickflow/icons/chevron-down.svg` and `:/clickflow/icons/chevron-up.svg`.

- [ ] **Step 1: Write the failing resource test**

Add `controlChevronResourcesAreAvailable()` to `MainWindowTests`:

```cpp
void MainWindowTests::controlChevronResourcesAreAvailable() {
  QVERIFY(QFile::exists(":/clickflow/icons/chevron-down.svg"));
  QVERIFY(QFile::exists(":/clickflow/icons/chevron-up.svg"));
}
```

Add `#include <QFile>` and register the function in `private slots`.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests --parallel
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build/windows-vs2026-debug -C Debug -R '^MainWindow$' --output-on-failure
```

Expected: `MainWindow` fails because both `:/clickflow/icons/...` resources do not exist.

- [ ] **Step 3: Add the minimal vector resources**

Create both SVGs with a `12 × 8` view box, no fill, `#526070` stroke,
`1.6` stroke width, round line caps and round joins:

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="12" height="8" viewBox="0 0 12 8">
  <path d="M1.5 1.5 6 6l4.5-4.5" fill="none" stroke="#526070"
        stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
```

Use the reversed path `M1.5 6.5 6 2l4.5 4.5` for the up chevron.

Create the resource collection:

```xml
<RCC>
  <qresource prefix="/clickflow/icons">
    <file alias="chevron-down.svg">chevron-down.svg</file>
    <file alias="chevron-up.svg">chevron-up.svg</file>
  </qresource>
</RCC>
```

Add `src/app/resources/ClickFlowResources.qrc` to `SHELL_WIDGET_SOURCES` so
both the application and `QtClickerMainWindowTests` link the resources.

- [ ] **Step 4: Reconfigure and run the resource test**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --preset windows-local
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests --parallel
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build/windows-vs2026-debug -C Debug -R '^MainWindow$' --output-on-failure
```

Expected: `MainWindow` passes.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt tests/MainWindowTests.cpp src/app/resources
git commit -m "feat: add ClickFlow control icons"
```

---

### Task 2: Apply the soft control buttons and frameless sidebar

**Files:**
- Modify: `src/app/MainWindow.cpp`
- Test: `tests/MainWindowTests.cpp`

**Interfaces:**
- Consumes: `:/clickflow/icons/chevron-down.svg` and `:/clickflow/icons/chevron-up.svg`.
- Produces: the application-level QSS contract for combo boxes, spin boxes and `#sidebarNavigation`.

- [ ] **Step 1: Write the failing style-contract test**

Add `usesClickFlowControlChrome()` to `MainWindowTests`:

```cpp
void MainWindowTests::usesClickFlowControlChrome() {
  const QString appName =
      QString("QtClickerMainWindowTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository));

  const QString style = window.styleSheet();
  QVERIFY(style.contains("QComboBox::down-arrow"));
  QVERIFY(style.contains("QSpinBox::up-button"));
  QVERIFY(style.contains("QSpinBox::down-button"));
  QVERIFY(style.contains(":/clickflow/icons/chevron-down.svg"));
  QVERIFY(style.contains(":/clickflow/icons/chevron-up.svg"));
  QVERIFY(style.contains("#sidebarNavigation"));
  QVERIFY(style.contains("border: none"));
}
```

Register the function in `private slots`.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests --parallel
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build/windows-vs2026-debug -C Debug -R '^MainWindow$' --output-on-failure
```

Expected: `MainWindow` fails at the first missing control subcontrol selector.

- [ ] **Step 3: Replace the generic widget style with explicit controls**

In `MainWindow::buildUi()`, remove `QListWidget` from the generic input
selector and add these focused rules:

```css
#sidebarNavigation {
  background: transparent;
  border: none;
  outline: none;
}

QComboBox, QSpinBox, QKeySequenceEdit {
  min-height: 30px;
  border: 1px solid #cfd5dd;
  border-radius: 7px;
  background: white;
  padding: 2px 34px 2px 10px;
}
QComboBox:hover, QSpinBox:hover, QKeySequenceEdit:hover {
  border-color: #9eabc0;
}
QComboBox:focus, QSpinBox:focus, QKeySequenceEdit:focus {
  border: 1px solid #2563eb;
}
QComboBox::drop-down {
  subcontrol-origin: padding;
  subcontrol-position: top right;
  width: 30px;
  margin: 3px;
  border: none;
  border-radius: 5px;
}
QComboBox::drop-down:hover {
  background: #edf3ff;
}
QComboBox::down-arrow {
  image: url(:/clickflow/icons/chevron-down.svg);
  width: 12px;
  height: 8px;
}
QSpinBox {
  padding-right: 32px;
}
QSpinBox::up-button, QSpinBox::down-button {
  subcontrol-origin: border;
  width: 28px;
  border: none;
  background: transparent;
}
QSpinBox::up-button {
  subcontrol-position: top right;
  margin: 3px 3px 0 0;
  border-top-left-radius: 5px;
  border-top-right-radius: 5px;
}
QSpinBox::down-button {
  subcontrol-position: bottom right;
  margin: 0 3px 3px 0;
  border-bottom-left-radius: 5px;
  border-bottom-right-radius: 5px;
}
QSpinBox::up-button:hover, QSpinBox::down-button:hover {
  background: #edf3ff;
}
QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
  background: #dce8ff;
}
QSpinBox::up-arrow {
  image: url(:/clickflow/icons/chevron-up.svg);
  width: 10px;
  height: 6px;
}
QSpinBox::down-arrow {
  image: url(:/clickflow/icons/chevron-down.svg);
  width: 10px;
  height: 6px;
}
QComboBox:disabled, QSpinBox:disabled, QKeySequenceEdit:disabled {
  color: #8a94a3;
  background: #f5f6f8;
}
```

Keep the existing sidebar selected-row and start/stop button rules unchanged.

- [ ] **Step 4: Run the focused and full tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build/windows-vs2026-debug --config Debug --parallel
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

Expected: all seven CTest tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/app/MainWindow.cpp tests/MainWindowTests.cpp
git commit -m "style: polish ClickFlow form controls"
```

---

### Task 3: Windows visual verification

**Files:**
- Verify: `build/windows-vs2026-debug/Debug/ClickFlow.exe`
- Verify: `dist/` remains unchanged

**Interfaces:**
- Consumes: the built ClickFlow Debug executable and installed Qt 6.8.3 runtime.
- Produces: visual evidence that the new control chrome renders correctly at the current Windows DPI.

- [ ] **Step 1: Launch one visible Debug instance**

Terminate only ClickFlow instances started during this verification, then launch
`build/windows-vs2026-debug/Debug/ClickFlow.exe` with
`D:\Qt\6.8.3\msvc2022_64\bin` prepended to that process's PATH.

- [ ] **Step 2: Capture the DPI-correct window**

Use `DwmGetWindowAttribute` with `DWMWA_EXTENDED_FRAME_BOUNDS` and
`PrintWindow(PW_RENDERFULLCONTENT)` so the bitmap uses physical pixels.

- [ ] **Step 3: Check the approved states**

Confirm from the real screenshot:

- combo boxes show one small chevron with no black divider;
- spin boxes retain two distinct adjustment targets with soft chevrons;
- disabled coordinate and click-count controls remain readable;
- the sidebar navigation has no surrounding rectangle;
- selected navigation and the main action button remain the only strong blue areas;
- the bottom action bar and all form rows remain visible.

- [ ] **Step 4: Run final verification**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --preset windows-local
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build/windows-vs2026-debug --config Debug --parallel
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
git diff --check
git status --short
git status --short -- dist
```

Expected: configure and build succeed, all seven tests pass, `git diff --check`
prints no errors, `dist` has no changes, and the only unrelated status entry is
the user's untracked `.vscode/launch.json`.
