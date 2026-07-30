# ClickFlow Manual Hotkey Activation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prevent ClickFlow from registering global hotkeys at startup and require an explicit per-session activation switch.

**Architecture:** `MainWindow` owns a non-persistent `globalHotkeysEnabled_` session flag and coordinates registration. `HotkeySettingsPage` exposes a checkbox and status label but never calls platform services directly; platform implementations remain unchanged.

**Tech Stack:** C++20, Qt 6 Widgets, Qt Test, CMake

## Global Constraints

- Every application launch starts with global hotkeys disabled.
- Disabled means `registerHotkeys` is not called and no system hotkey is occupied.
- The enabled state is never persisted.
- UI click actions continue to work while global hotkeys are disabled.
- Registration is all-or-nothing; failure releases the full group and returns the switch to off.
- Existing control sizing work remains intact: standard controls are 40px and primary actions are 44px.

---

### Task 1: Add the manual activation control

**Files:**
- Modify: `tests/ClickFlowPageTests.cpp`
- Modify: `src/app/pages/HotkeySettingsPage.h`
- Modify: `src/app/pages/HotkeySettingsPage.cpp`

**Interfaces:**
- Produces: `void activationRequested(bool enabled)`, `void setActivationState(bool enabled, const QString& status)`, and `bool activationEnabled() const`.

- [ ] **Step 1: Write the failing page test**

Add `hotkeyActivationIsExplicit()` to `ClickFlowPageTests`. Construct
`HotkeySettingsPage`, find `QCheckBox` object `globalHotkeysEnabledCheck` and
`QLabel` object `globalHotkeysStatusLabel`, and assert:

```cpp
QVERIFY(toggle);
QVERIFY(status);
QVERIFY(!toggle->isChecked());
QCOMPARE(status->text(), QString("当前未占用任何系统热键"));

QSignalSpy activationSpy(&page, &HotkeySettingsPage::activationRequested);
toggle->click();
QCOMPARE(activationSpy.count(), 1);
QCOMPARE(activationSpy.takeFirst().at(0).toBool(), true);

page.setActivationState(false, "启用失败");
QVERIFY(!page.activationEnabled());
QCOMPARE(status->text(), QString("启用失败"));
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
cmake --build build --target ClickFlowPageTests --parallel
```

Expected: compilation fails because the activation API does not exist.

- [ ] **Step 3: Implement the page API**

Add a `QCheckBox` and status `QLabel` above the hotkey editors. Give them the
object names from Step 1. Connect checkbox `toggled` to `activationRequested`.
Implement `setActivationState` with `QSignalBlocker` so programmatic rollback
does not emit another request. Make `setEditingEnabled` enable or disable only
the five `QKeySequenceEdit` controls so the activation switch remains usable.

- [ ] **Step 4: Run the focused page test and verify GREEN**

Run:

```bash
cmake --build build --target ClickFlowPageTests --parallel
./build/ClickFlowPageTests hotkeyActivationIsExplicit
```

Expected: PASS.

### Task 2: Enforce session-only registration in MainWindow

**Files:**
- Modify: `tests/MainWindowTests.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `HotkeySettingsPage::activationRequested(bool)` and `setActivationState`.
- Produces: `MainWindow::handleHotkeyActivationRequested(bool)`, `tryEnableGlobalHotkeys(const ClickProfile&)`, `disableGlobalHotkeys(const QString&)`, and `bool globalHotkeysEnabled_ = false`.

- [ ] **Step 1: Make MainWindow tests runnable on macOS**

Guard the Windows platform includes and `windowsFactoriesCreateNativeServices`
test with `#if defined(Q_OS_WIN)`. Define `QtClickerMainWindowTests` outside the
Windows-only CMake block using `MainWindow.cpp`, `UiStyle.cpp`, shell sources,
page sources, and `${PLATFORM_SOURCES}`. Link ApplicationServices and Carbon on
Apple; link `user32` and `dwmapi` on Windows. Keep `QT_QPA_PLATFORM=offscreen`.

- [ ] **Step 2: Write failing lifecycle tests**

Extend `MainWindowFakeHotkeyService` with:

```cpp
bool registrationResult = true;
int registerCount = 0;
int unregisterCount = 0;
ClickProfile lastRegisteredProfile;
```

Increment the counters in its overrides. Add tests proving:

```cpp
QCOMPARE(hotkeys->registerCount, 0);  // immediately after construction
startButton->click();
QCOMPARE(hotkeys->registerCount, 0);  // UI start does not opt in

toggle->click();
QCOMPARE(hotkeys->registerCount, 1);
QVERIFY(toggle->isChecked());
toggle->click();
QCOMPARE(hotkeys->unregisterCount, 1);
QVERIFY(!toggle->isChecked());
```

Add a registration-failure case with `registrationResult = false` and assert
the toggle rolls back to off.

- [ ] **Step 3: Run MainWindow tests and verify RED**

Run:

```bash
cmake --preset macos-debug
cmake --build build --target QtClickerMainWindowTests --parallel
./build/QtClickerMainWindowTests startsWithGlobalHotkeysDisabled
```

Expected: FAIL because construction currently calls `registerHotkeys`.

- [ ] **Step 4: Implement MainWindow ownership**

Connect `activationRequested` to `handleHotkeyActivationRequested`. Remove the
constructor registration call and initialize both pages with registration
state false. Remove implicit registration from `handleStartStop`.

When activation is requested:

```cpp
if (!enabled) {
  disableGlobalHotkeys("当前未占用任何系统热键");
  return;
}
tryEnableGlobalHotkeys(collectProfileFromUi());
```

`tryEnableGlobalHotkeys` validates, registers once, updates
`globalHotkeysEnabled_`, updates both pages, and saves valid settings.
`disableGlobalHotkeys` calls `unregisterAll` only for an active or failed
registration attempt, sets the flag false, and updates page status.

When hotkeys are edited or a preset is loaded, register only if
`globalHotkeysEnabled_` is true. Otherwise save a valid configuration without
calling the platform service.

- [ ] **Step 5: Run focused and full tests**

Run:

```bash
cmake --build --preset build --parallel
./build/QtClickerMainWindowTests
ctest --preset test
git diff --check
```

Expected: all tests pass with zero failures.

- [ ] **Step 6: Perform a startup smoke test**

Launch `build/ClickFlow.app`, verify the hotkey page shows the switch off and
“当前未占用任何系统热键”, and confirm the configured key remains usable in a
normal text field until the user manually enables global hotkeys.

- [ ] **Step 7: Commit implementation**

```bash
git add CMakeLists.txt src/app/MainWindow.h src/app/MainWindow.cpp \
  src/app/pages/HotkeySettingsPage.h src/app/pages/HotkeySettingsPage.cpp \
  tests/MainWindowTests.cpp tests/ClickFlowPageTests.cpp \
  src/app/UiStyle.h src/app/UiStyle.cpp \
  src/app/pages/MacroRecordingPage.cpp
git commit -m "fix: require manual global hotkey activation"
```
