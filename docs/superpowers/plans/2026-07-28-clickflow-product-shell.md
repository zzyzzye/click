# ClickFlow Product Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the temporary single-column “极简连点器” UI with the ClickFlow 0.2.0 product shell: an iPad Settings-style sidebar, three focused pages, persistent status/action regions, and one CMake-owned version source.

**Architecture:** Keep `MainWindow` responsible for application services and orchestration, while moving presentation and profile editing into focused widgets with profile-oriented interfaces. Generate application and Windows metadata from the CMake project version, while retaining the `QtClicker` QSettings application key for backward compatibility.

**Tech Stack:** C++20, Qt 6.8.3 Widgets/Test, CMake 3.24+, generated C++/Windows resource files, CTest

## Global Constraints

- The user-visible product name is `ClickFlow`.
- The application version is `0.2.0` and follows Semantic Versioning.
- `project(ClickFlow VERSION 0.2.0 LANGUAGES CXX)` is the single version source.
- The existing QSettings application key remains `QtClicker`.
- Navigation contains exactly `连点设置`, `热键`, and `预设与关于`.
- Status and Start/Stop controls remain visible on every page.
- Existing profile serialization, platform backends, and F6/F7/F8 defaults remain unchanged.
- Do not add packaging, update checking, telemetry, or release-download behavior.
- Do not run the Windows packaging script during this implementation.
- Keep `/dist/` and `CMakeUserPresets.json` ignored.

---

## Planned File Structure

- `CMakeLists.txt`: ClickFlow 0.2.0 source of truth, generated include/resource
  setup, UI/test source registration, and executable output name.
- `src/app/ClickFlowVersion.h.in`: generated version constants.
- `src/app/ClickFlow.rc.in`: generated Windows VERSIONINFO metadata.
- `src/app/AppIdentity.h/.cpp`: apply product display name and version while
  preserving the QSettings key.
- `src/app/widgets/NavigationSidebar.h/.cpp`: brand header and three-page
  navigation.
- `src/app/widgets/StatusStrip.h/.cpp`: permission, status, and run-progress
  summary.
- `src/app/widgets/ActionBar.h/.cpp`: persistent mode summary and Start/Stop
  action.
- `src/app/pages/ClickSettingsPage.h/.cpp`: click and run-behavior profile
  fields.
- `src/app/pages/HotkeySettingsPage.h/.cpp`: three global hotkey fields.
- `src/app/pages/PresetsAboutPage.h/.cpp`: preset list/actions and About
  metadata.
- `src/app/MainWindow.h/.cpp`: compose the shell and coordinate services.
- `src/app/main.cpp`: apply centralized identity before creating the window.
- `tests/AppIdentityTests.cpp`: product/version propagation tests.
- `tests/ProductShellTests.cpp`: component and navigation tests.
- `tests/MainWindowTests.cpp`: integrated profile, status, and running-state
  tests.
- `README.md`: product name and version-oriented development instructions only;
  no new packaging workflow.

### Task 1: Establish ClickFlow 0.2.0 as the Single Product Identity

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/app/ClickFlowVersion.h.in`
- Create: `src/app/ClickFlow.rc.in`
- Create: `src/app/AppIdentity.h`
- Create: `src/app/AppIdentity.cpp`
- Modify: `src/app/main.cpp`
- Create: `tests/AppIdentityTests.cpp`

**Interfaces:**
- Produces: `ClickFlowVersion::string`, `ClickFlowVersion::major`,
  `ClickFlowVersion::minor`, `ClickFlowVersion::patch`
- Produces: `void applyApplicationIdentity()`
- Preserves: `QCoreApplication::applicationName() == "QtClicker"`

- [ ] **Step 1: Write the failing identity test**

Create `tests/AppIdentityTests.cpp` with literal expectations:

```cpp
#include <QCoreApplication>
#include <QTest>

#include "app/AppIdentity.h"
#include "ClickFlowVersion.h"

class AppIdentityTests : public QObject {
  Q_OBJECT
 private slots:
  void appliesClickFlowIdentity();
};

void AppIdentityTests::appliesClickFlowIdentity() {
  applyApplicationIdentity();
  QCOMPARE(QCoreApplication::organizationName(), QString("OpenAI"));
  QCOMPARE(QCoreApplication::applicationName(), QString("QtClicker"));
  QCOMPARE(QCoreApplication::applicationVersion(), QString("0.2.0"));
  QCOMPARE(QGuiApplication::applicationDisplayName(), QString("ClickFlow"));
  QCOMPARE(QString(ClickFlowVersion::string), QString("0.2.0"));
}
```

Register a separate `ClickFlowIdentityTests` target so it does not add a second
QTest main to an existing executable.

- [ ] **Step 2: Run the test and verify RED**

```powershell
cmake --preset windows-local
cmake --build build/windows-vs2026-debug --config Debug `
  --target ClickFlowIdentityTests
```

Expected: FAIL because `AppIdentity.h` and `ClickFlowVersion.h` do not exist.

- [ ] **Step 3: Generate the version header**

Change the project line to:

```cmake
project(ClickFlow VERSION 0.2.0 LANGUAGES CXX)
```

Generate into `${CMAKE_CURRENT_BINARY_DIR}/generated`:

```cmake
configure_file(
  src/app/ClickFlowVersion.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/ClickFlowVersion.h
  @ONLY
)
```

The template defines:

```cpp
#pragma once
namespace ClickFlowVersion {
inline constexpr char string[] = "@PROJECT_VERSION@";
inline constexpr int major = @PROJECT_VERSION_MAJOR@;
inline constexpr int minor = @PROJECT_VERSION_MINOR@;
inline constexpr int patch = @PROJECT_VERSION_PATCH@;
}
```

Add the generated directory to application and test include paths.

- [ ] **Step 4: Implement application identity**

`applyApplicationIdentity()` sets:

```cpp
QCoreApplication::setOrganizationName("OpenAI");
QCoreApplication::setApplicationName("QtClicker");
QCoreApplication::setApplicationVersion(ClickFlowVersion::string);
QGuiApplication::setApplicationDisplayName("ClickFlow");
```

Call it in `main.cpp` immediately after `QApplication` construction. Remove the
two duplicated identity setters currently in `main.cpp`.

- [ ] **Step 5: Generate Windows file metadata**

Configure `ClickFlow.rc.in` only on Windows and add the generated `.rc` file to
the app target. Use numeric commas for `FILEVERSION`/`PRODUCTVERSION` and the
same CMake values for strings:

```rc
FILEVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
PRODUCTVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
VALUE "FileDescription", "ClickFlow 连点器\0"
VALUE "FileVersion", "@PROJECT_VERSION@\0"
VALUE "ProductName", "ClickFlow\0"
VALUE "ProductVersion", "@PROJECT_VERSION@\0"
```

Keep the CMake target name `QtClicker` but set:

```cmake
set_target_properties(QtClicker PROPERTIES OUTPUT_NAME ClickFlow)
```

- [ ] **Step 6: Run and pass identity tests**

```powershell
cmake --preset windows-local
cmake --build build/windows-vs2026-debug --config Debug `
  --target ClickFlowIdentityTests
ctest --test-dir build/windows-vs2026-debug -C Debug `
  -R AppIdentity --output-on-failure
```

Expected: PASS with all five literal identity assertions.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/app/ClickFlowVersion.h.in src/app/ClickFlow.rc.in `
  src/app/AppIdentity.* src/app/main.cpp tests/AppIdentityTests.cpp
git commit -m "feat: establish ClickFlow 0.2.0 identity"
```

### Task 2: Build the Persistent Product Shell Components

**Files:**
- Create: `src/app/widgets/NavigationSidebar.h`
- Create: `src/app/widgets/NavigationSidebar.cpp`
- Create: `src/app/widgets/StatusStrip.h`
- Create: `src/app/widgets/StatusStrip.cpp`
- Create: `src/app/widgets/ActionBar.h`
- Create: `src/app/widgets/ActionBar.cpp`
- Create: `tests/ProductShellTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `enum class ShellPage { ClickSettings, Hotkeys, PresetsAbout }`
- Produces: `NavigationSidebar::pageSelected(ShellPage)`
- Produces: `StatusStrip::setPermissionState(bool)`,
  `setStatus(QString)`, `setProgress(QString)`
- Produces: `ActionBar::setRunning(bool)`, `setSummary(QString)`,
  `startStopRequested()`

- [ ] **Step 1: Write failing sidebar behavior tests**

Create a test that asserts exactly three visible navigation rows, selects each
row, and checks the emitted enum:

```cpp
NavigationSidebar sidebar;
QSignalSpy spy(&sidebar, &NavigationSidebar::pageSelected);
QCOMPARE(sidebar.pageCount(), 3);
sidebar.setCurrentPage(ShellPage::Hotkeys);
QCOMPARE(sidebar.currentPage(), ShellPage::Hotkeys);
QCOMPARE(spy.last().first().value<ShellPage>(), ShellPage::Hotkeys);
QCOMPARE(sidebar.productName(), QString("ClickFlow"));
QCOMPARE(sidebar.versionText(), QString("0.2.0"));
```

Register the enum as a Qt metatype.

- [ ] **Step 2: Write failing persistent-region tests**

Test the observable labels and primary action:

```cpp
StatusStrip status;
status.setPermissionState(true);
status.setStatus("运行中");
status.setProgress("剩余 8 次");
QCOMPARE(status.permissionText(), QString("输入控制可用"));
QCOMPARE(status.statusText(), QString("运行中"));
QCOMPARE(status.progressText(), QString("剩余 8 次"));

ActionBar actions;
actions.setRunning(true);
QCOMPARE(actions.buttonText(), QString("停止连点"));
```

- [ ] **Step 3: Run the shell tests and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug `
  --target ClickFlowProductShellTests
```

Expected: FAIL because the three component classes do not exist.

- [ ] **Step 4: Implement `NavigationSidebar`**

Use a fixed-width `QFrame` with product/version labels and a `QListWidget`.
Store `ShellPage` integer values in item data. Expose behavior methods rather
than child widgets:

```cpp
int pageCount() const;
ShellPage currentPage() const;
void setCurrentPage(ShellPage page);
QString productName() const;
QString versionText() const;
```

The only entries are `连点设置`, `热键`, and `预设与关于`.

- [ ] **Step 5: Implement `StatusStrip` and `ActionBar`**

Both are focused `QFrame` subclasses. `ActionBar` emits
`startStopRequested()` from its single primary button and maps running state to
`开始连点`/`停止连点`. `StatusStrip` uses text plus a semantic property such as
`state="available"` rather than embedding colors in state logic.

- [ ] **Step 6: Pass shell tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug `
  --target ClickFlowProductShellTests
ctest --test-dir build/windows-vs2026-debug -C Debug `
  -R ProductShell --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/app/widgets tests/ProductShellTests.cpp
git commit -m "feat: add ClickFlow shell components"
```

### Task 3: Extract the Click Settings Page

**Files:**
- Create: `src/app/pages/ClickSettingsPage.h`
- Create: `src/app/pages/ClickSettingsPage.cpp`
- Create: `tests/ClickSettingsPageTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  `void ClickSettingsPage::setProfile(const ClickProfile&)`
- Produces:
  `void ClickSettingsPage::applyToProfile(ClickProfile&) const`
- Produces: `captureRequested()`, `alwaysOnTopChanged(bool)`
- Produces: `setEditingEnabled(bool)`, `summary()`

- [ ] **Step 1: Write a failing profile round-trip test**

Use a profile containing right click, fixed `(640, 480)`, finite count `8`,
jitter `7`, countdown `3`, and always-on-top. Call `setProfile`, apply into a
fresh profile, and compare every field owned by this page. The expected values
are literals, not derived from the page.

- [ ] **Step 2: Write failing dependency-state tests**

Assert fixed coordinate controls are enabled only for `FixedPoint`, repeat
count is enabled only for `Finite`, and clicking the capture control emits
`captureRequested()` once.

- [ ] **Step 3: Run tests and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug `
  --target ClickFlowClickSettingsTests
```

Expected: FAIL because `ClickSettingsPage` is missing.

- [ ] **Step 4: Implement two-card settings UI**

Use plain `QFrame` cards with:

```text
Basic Click: interval, button, target mode, X/Y, capture
Run Behavior: repeat mode, count, jitter, countdown, always-on-top
```

Use a shared two-column `QGridLayout` rhythm and no `QGroupBox`. Keep fixed and
finite dependent rows visible but disabled. Assign stable object names used by
existing integration tests.

- [ ] **Step 5: Implement profile mapping and summary**

`applyToProfile` modifies only interval, button, target mode, fixed point,
repeat mode/count, jitter, countdown, and always-on-top. `summary()` returns a
short Chinese string such as `100 毫秒 · 跟随鼠标 · 无限`.

- [ ] **Step 6: Pass page tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug `
  --target ClickFlowClickSettingsTests
ctest --test-dir build/windows-vs2026-debug -C Debug `
  -R ClickSettingsPage --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/app/pages/ClickSettingsPage.* `
  tests/ClickSettingsPageTests.cpp
git commit -m "refactor: extract ClickFlow click settings page"
```

### Task 4: Extract Hotkeys and Presets/About Pages

**Files:**
- Create: `src/app/pages/HotkeySettingsPage.h`
- Create: `src/app/pages/HotkeySettingsPage.cpp`
- Create: `src/app/pages/PresetsAboutPage.h`
- Create: `src/app/pages/PresetsAboutPage.cpp`
- Create: `tests/SecondaryPagesTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  `HotkeySettingsPage::setProfile(const ClickProfile&)`
- Produces:
  `HotkeySettingsPage::applyToProfile(ClickProfile&) const`
- Produces:
  `HotkeySettingsPage::validate(QString* error) const`
- Produces:
  `PresetsAboutPage::setPresetNames(QStringList, QString selected)`
- Produces: `selectedPresetName()`, `newRequested()`, `saveRequested()`,
  `renameRequested()`, `deleteRequested()`, `loadRequested()`

- [ ] **Step 1: Write failing hotkey page tests**

Round-trip Ctrl+F6/Ctrl+F7/Ctrl+F8 and assert validation rejects empty,
duplicate, and multi-stroke sequences with the offending binding named in the
error.

- [ ] **Step 2: Write failing preset/About tests**

Set names `Alpha`, `Beta` with `Beta` selected. Assert selection-dependent
Rename/Delete/Load actions are enabled, selection returns `Beta`, and the About
values are `ClickFlow`, `0.2.0`, the current Qt version, and a non-empty platform
string.

- [ ] **Step 3: Run tests and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug `
  --target ClickFlowSecondaryPagesTests
```

Expected: FAIL because both page classes are missing.

- [ ] **Step 4: Implement `HotkeySettingsPage`**

Create one card containing the three `QKeySequenceEdit` rows and concise help
copy. Move existing hotkey validation from `MainWindow` into this page without
changing accepted sequences.

- [ ] **Step 5: Implement `PresetsAboutPage`**

Create an upper presets card with a compact horizontal action toolbar and a
lower About card. Read version from
`QCoreApplication::applicationVersion()` and platform from compile-time Qt OS
macros. Use `qVersion()` for the Qt version.

- [ ] **Step 6: Pass secondary-page tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug `
  --target ClickFlowSecondaryPagesTests
ctest --test-dir build/windows-vs2026-debug -C Debug `
  -R SecondaryPages --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/app/pages/HotkeySettingsPage.* `
  src/app/pages/PresetsAboutPage.* tests/SecondaryPagesTests.cpp
git commit -m "refactor: extract ClickFlow secondary pages"
```

### Task 5: Recompose `MainWindow` Around the New Shell

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `tests/MainWindowTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: all components and page interfaces from Tasks 2–4
- Preserves: current constructor injection for backend, hotkeys, and repository
- Produces: one three-page `QStackedWidget` shell

- [ ] **Step 1: Write failing integrated shell tests**

Extend `MainWindowTests` to assert:

```cpp
QCOMPARE(window.windowTitle(), QString("ClickFlow"));
QCOMPARE(window.minimumSize(), QSize(820, 560));
QVERIFY(window.findChild<NavigationSidebar*>());
QVERIFY(window.findChild<StatusStrip*>());
QVERIFY(window.findChild<ActionBar*>());
QCOMPARE(window.findChild<QStackedWidget*>()->count(), 3);
```

Switch all three sidebar entries and assert the stacked index changes while the
same status strip and action bar remain visible.

- [ ] **Step 2: Preserve and run the existing profile behavior test as RED**

Update selectors to page component APIs, not individual controls. The existing
loaded-profile/start behavior must still deliver the complete expected profile
to the fake backend.

Run:

```powershell
cmake --build build/windows-vs2026-debug --config Debug `
  --target QtClickerMainWindowTests
ctest --test-dir build/windows-vs2026-debug -C Debug `
  -R MainWindow --output-on-failure
```

Expected: FAIL because the current main window still has the old one-column
group-box layout.

- [ ] **Step 3: Replace `buildUi()` with shell composition**

Build:

```text
QHBoxLayout
├─ NavigationSidebar
└─ content QVBoxLayout
   ├─ StatusStrip
   ├─ QStackedWidget (three pages)
   └─ ActionBar
```

Set default size `920x620`, minimum `820x560`, window title `ClickFlow`, and the
default page to `ClickSettings`.

- [ ] **Step 4: Move profile collection and application to pages**

`applyProfileToUi()` calls both page `setProfile` methods.
`collectProfileFromUi()` starts with the current profile name, then asks both
pages to apply their fields. Keep settings repository behavior unchanged.

- [ ] **Step 5: Rewire controller and platform state**

- status callbacks update `StatusStrip`;
- capture request reads the backend cursor and calls
  `ClickSettingsPage::setFixedPoint`;
- Start/Stop comes from `ActionBar`;
- running state disables page editing and preset mutations but not navigation;
- `ActionBar` summary refreshes when click settings change;
- permission action remains macOS-only.

- [ ] **Step 6: Apply restrained product styling**

Apply one root stylesheet scoped by object names/properties. Use neutral
surfaces, one accent color, 8/16/24 spacing, subtle card borders, and a clear
primary button. Do not set fixed heights on text-bearing rows. Remove all
`QGroupBox` construction and the permanent “怎么用” block.

- [ ] **Step 7: Pass integrated and full tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --parallel
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 8: Commit**

```powershell
git add CMakeLists.txt src/app/MainWindow.* tests/MainWindowTests.cpp
git commit -m "feat: adopt ClickFlow sidebar layout"
```

### Task 6: Documentation and Non-Packaging Verification

**Files:**
- Modify: `README.md`
- Modify only on verified failures: UI/component/test files from Tasks 1–5

**Interfaces:**
- Verifies the approved ClickFlow design without creating or modifying `dist/`

- [ ] **Step 1: Update product/version documentation**

Change the README title and introduction to ClickFlow 0.2.0. Document that
`QtClicker` remains the settings compatibility key and that the CMake project
version is the only version source. Do not add a release claim.

- [ ] **Step 2: Run fresh configure, build, and tests**

```powershell
cmake --preset windows-local
cmake --build build/windows-vs2026-debug --config Debug --parallel
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

Expected: configure/build exit `0`; CTest reports zero failures.

- [ ] **Step 3: Verify executable metadata without packaging**

Confirm the build output is named `ClickFlow.exe`. Read its Windows version
metadata and verify product/file version `0.2.0`. Do not call
`package-windows.ps1` and do not write to `dist/`.

- [ ] **Step 4: Perform visual QA on the Debug app**

Launch the Debug app with a temporary Qt DLL lookup path. At approximately
920x620, and at 125% Windows display scaling when the current desktop supports
changing scale without disrupting the user session, verify:

1. no startup modal;
2. exactly three sidebar entries;
3. no clipped labels or unexpected scrollbar;
4. settings cards contain no nested group-box borders;
5. status and action bar remain visible on every page;
6. preset list does not occupy the default page;
7. edit locks apply while running and navigation remains available.

Do not trigger real continuous clicking during visual QA.

- [ ] **Step 5: Check repository hygiene**

```powershell
git diff --check
git status --short
git log --oneline -8
```

Expected: local `dist/` remains ignored and untouched; unrelated
`.vscode/launch.json` remains untracked unless the user separately chooses to
add it.

- [ ] **Step 6: Commit documentation**

```powershell
git add README.md
git commit -m "docs: describe ClickFlow 0.2.0 development"
```

- [ ] **Step 7: Record macOS verification boundary**

If no macOS runner is available, report structural preservation only. Do not
claim macOS visual or runtime verification from the Windows machine.
