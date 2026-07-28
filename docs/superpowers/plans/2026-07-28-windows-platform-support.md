# Windows Platform Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a Windows 10/11 x64 build with the same mouse-clicking, profile, hotkey, and window behavior as the macOS application, using Qt 6.8.3 and MSVC 2022 without regressing macOS.

**Architecture:** Keep `ClickBackend`, `HotkeyService`, and `PlatformServices` as the cross-platform seams. Put Win32 side effects behind narrow injectable adapters, keep key translation pure, and select platform sources in CMake. Complete the existing UI stubs so every already-modeled profile field is configurable on both platforms.

**Tech Stack:** C++20, Qt 6.8.3 Widgets/Test, CMake 3.24+, MSVC 2022, Win32 `SendInput`/`RegisterHotKey`, CTest

## Global Constraints

- Support Windows 10 and Windows 11 on x64 with MSVC 2022 and Qt 6.8.3.
- Preserve the macOS implementation, settings schema, and default hotkeys.
- Use public Qt 6 and Win32 APIs; do not use private Qt headers.
- Do not require administrator privileges or add third-party runtime dependencies.
- Do not hard-code developer-specific Qt paths in committed build files.
- Keep Windows headers and native handles out of cross-platform headers.
- Test system side effects through dependency-injectable adapters.

---

## Planned File Structure

- `CMakeLists.txt`: declare languages and source sets per platform; define core,
  platform, app, and test targets.
- `CMakePresets.json`: add portable Windows configure/build/test presets.
- `src/platform/windows/WindowsInputApi.h/.cpp`: narrow Win32 cursor and mouse
  event boundary.
- `src/platform/windows/WindowsClickBackend.h/.cpp`: profile-to-click behavior.
- `src/platform/windows/WindowsHotkeyMapping.h/.cpp`: pure portable-sequence to
  Win32 mapping.
- `src/platform/windows/WindowsHotkeyApi.h/.cpp`: narrow global registration
  boundary.
- `src/platform/windows/WindowsHotkeyService.h/.cpp`: transactional registration,
  Qt native message dispatch, and signals.
- `src/platform/windows/PlatformServicesWindows.cpp`: Windows factories.
- `src/app/MainWindow.h/.cpp`: expose and persist all existing profile fields,
  platform-neutral permission copy.
- `tests/ClickerTests.cpp`: retain core and settings tests.
- `tests/WindowsClickBackendTests.cpp`: safe tests using a fake input adapter.
- `tests/WindowsHotkeyTests.cpp`: mapping, rollback, and dispatch tests.
- `tests/MainWindowTests.cpp`: cross-platform profile-control and permission-copy
  tests using injected services.
- `scripts/package-windows.ps1`: deterministic `windeployqt` staging command.
- `README.md`: Windows configure, build, test, run, and package instructions.

### Task 1: Make the Build Graph Platform-Aware

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Delete: `src/platform/windows/WindowsPlaceholder.txt`

**Interfaces:**
- Produces: targets `QtClickerCore`, `QtClicker`, `QtClickerTests`,
  `QtClickerMainWindowTests`, and Windows-only `QtClickerWindowsTests`
- Consumes: `CMAKE_PREFIX_PATH` supplied externally or by
  `CMakeUserPresets.json`

- [ ] **Step 1: Capture the failing Windows configuration**

Run:

```powershell
cmake -S . -B build/windows-red -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=D:\Qt\6.8.3\msvc2022_64
```

Expected: FAIL because the project globally requires `OBJCXX` and includes
macOS-only `.mm` sources.

- [ ] **Step 2: Split common and platform source sets**

Change the project declaration and platform branches to this shape:

```cmake
project(QtClicker VERSION 0.1.0 LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
  set(PLATFORM_SOURCES
    src/platform/macos/MacOSClickBackend.mm
    src/platform/macos/MacOSHotkeyService.mm
    src/platform/macos/PlatformServicesMac.mm)
elseif(WIN32)
  set(PLATFORM_SOURCES
    src/platform/windows/WindowsInputApi.cpp
    src/platform/windows/WindowsClickBackend.cpp
    src/platform/windows/WindowsHotkeyMapping.cpp
    src/platform/windows/WindowsHotkeyApi.cpp
    src/platform/windows/WindowsHotkeyService.cpp
    src/platform/windows/PlatformServicesWindows.cpp)
else()
  message(FATAL_ERROR "QtClicker supports only macOS and Windows.")
endif()
```

Move the existing core sources into `QtClickerCore`, link it publicly to
`Qt6::Core`, and link the app/tests to that target. Link Windows platform code
to `user32`.

- [ ] **Step 3: Add portable Windows presets**

Add `windows-msvc-debug`, `windows-build`, and `windows-test`. The configure
preset uses the Visual Studio generator and `x64`, but does not set
`CMAKE_PREFIX_PATH`:

```json
{
  "name": "windows-msvc-debug",
  "generator": "Visual Studio 17 2022",
  "architecture": "x64",
  "binaryDir": "${sourceDir}/build/windows-msvc-debug",
  "cacheVariables": { "CMAKE_CONFIGURATION_TYPES": "Debug" },
  "condition": {
    "type": "equals",
    "lhs": "${hostSystemName}",
    "rhs": "Windows"
  }
}
```

- [ ] **Step 4: Verify the new graph reaches missing Windows sources**

Run the configuration command from Step 1 again.

Expected: FAIL only because the planned Windows source files do not exist yet;
there must be no Objective-C++ or Apple framework error.

- [ ] **Step 5: Commit the build boundary**

```powershell
git add CMakeLists.txt CMakePresets.json src/platform/windows/WindowsPlaceholder.txt
git commit -m "build: select platform sources in CMake"
```

### Task 2: Implement the Windows Click Backend with Safe Test Doubles

**Files:**
- Create: `src/platform/windows/WindowsInputApi.h`
- Create: `src/platform/windows/WindowsInputApi.cpp`
- Create: `src/platform/windows/WindowsClickBackend.h`
- Create: `src/platform/windows/WindowsClickBackend.cpp`
- Create: `tests/WindowsClickBackendTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  `WindowsInputApi::cursorPosition() -> std::optional<QPoint>`,
  `WindowsInputApi::clickAt(const QPoint&, ClickButton) -> bool`,
  `WindowsClickBackend(std::unique_ptr<WindowsInputApi>, RandomOffset)`
- Consumes: `ClickBackend`, `ClickProfile`

- [ ] **Step 1: Write failing fixed/follow cursor and button tests**

Use a fake adapter that records calls:

```cpp
class FakeWindowsInputApi final : public WindowsInputApi {
 public:
  std::optional<QPoint> cursorPosition() const override { return cursor; }
  bool clickAt(const QPoint& point, ClickButton button) override {
    clickedPoint = point;
    clickedButton = button;
    return clickResult;
  }
  std::optional<QPoint> cursor = QPoint(40, 50);
  QPoint clickedPoint;
  ClickButton clickedButton = ClickButton::Left;
  bool clickResult = true;
};
```

Add tests proving follow-cursor uses `(40, 50)`, fixed-point uses the profile
coordinate, right-button selection is preserved, and a `nullopt` cursor or
failed `clickAt` returns `false`.

- [ ] **Step 2: Run the click backend tests and verify RED**

```powershell
cmake --build build/windows-msvc-debug --config Debug --target QtClickerWindowsTests
ctest --test-dir build/windows-msvc-debug -C Debug -R WindowsClickBackend --output-on-failure
```

Expected: FAIL to compile because the adapter and backend do not exist.

- [ ] **Step 3: Implement the injectable backend**

Define:

```cpp
using RandomOffset = std::function<int(int minimum, int maximumInclusive)>;

class WindowsClickBackend final : public ClickBackend {
 public:
  explicit WindowsClickBackend(
      std::unique_ptr<WindowsInputApi> api = createNativeWindowsInputApi(),
      RandomOffset randomOffset = defaultRandomOffset);
  bool click(const ClickProfile& profile) override;
  QPoint currentCursorPosition() const override;
  bool hasAccessibilityPermission() const override { return true; }
  void requestAccessibilityPermission() override {}
};
```

Resolve the base point from cursor/fixed mode, apply two inclusive random
offsets when `jitterRadius > 0`, and call `clickAt` exactly once.

- [ ] **Step 4: Implement the native adapter**

`cursorPosition()` wraps `GetCursorPos`. `clickAt()` first calls `SetCursorPos`,
then sends exactly two `INPUT` records through one `SendInput` call:

```cpp
const DWORD down = button == ClickButton::Left
    ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
const DWORD up = button == ClickButton::Left
    ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
```

Return `false` unless cursor positioning succeeds and `SendInput` reports both
records inserted.

- [ ] **Step 5: Add and pass jitter-bound tests**

Inject lambdas returning the requested minimum and maximum. Verify a radius of
`7` produces base offsets `(-7, -7)` and `(+7, +7)`, while radius zero never
calls the random provider.

Run:

```powershell
cmake --build build/windows-msvc-debug --config Debug --target QtClickerWindowsTests
ctest --test-dir build/windows-msvc-debug -C Debug -R WindowsClickBackend --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add CMakeLists.txt src/platform/windows/WindowsInputApi.* `
  src/platform/windows/WindowsClickBackend.* tests/WindowsClickBackendTests.cpp
git commit -m "feat: add testable Windows click backend"
```

### Task 3: Add Pure Windows Hotkey Translation

**Files:**
- Create: `src/platform/windows/WindowsHotkeyMapping.h`
- Create: `src/platform/windows/WindowsHotkeyMapping.cpp`
- Create: `tests/WindowsHotkeyTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  `std::optional<WindowsHotkeyBinding> mapWindowsHotkey(const QString&)`
- `WindowsHotkeyBinding` contains `quint32 modifiers` and `quint32 virtualKey`

- [ ] **Step 1: Write table-driven mapping tests**

Cover F1/F12, A/Z, 0/9, Space, Enter, Return, Escape and every modifier. Include:

```cpp
QTest::newRow("ctrl-shift-f6")
    << QString("Ctrl+Shift+F6")
    << quint32(MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT)
    << quint32(VK_F6);
```

Add rejection tests for empty input, unsupported keys, and multi-stroke
sequences such as `"Ctrl+K, Ctrl+C"`.

- [ ] **Step 2: Run the mapping tests and verify RED**

```powershell
cmake --build build/windows-msvc-debug --config Debug --target QtClickerWindowsTests
ctest --test-dir build/windows-msvc-debug -C Debug -R WindowsHotkeyMapping --output-on-failure
```

Expected: FAIL to compile because `mapWindowsHotkey` is missing.

- [ ] **Step 3: Implement minimal pure translation**

Parse with `QKeySequence::PortableText`, require exactly one combination, map
Qt Shift/Control/Alt/Meta to Win32 modifier flags, and map only the key range
declared by the design. Always include `MOD_NOREPEAT`.

- [ ] **Step 4: Run and pass the mapping tests**

Run the Step 2 commands.

Expected: PASS for every supported and rejected case.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt src/platform/windows/WindowsHotkeyMapping.* `
  tests/WindowsHotkeyTests.cpp
git commit -m "feat: map Qt sequences to Windows hotkeys"
```

### Task 4: Implement Transactional Windows Global Hotkeys

**Files:**
- Create: `src/platform/windows/WindowsHotkeyApi.h`
- Create: `src/platform/windows/WindowsHotkeyApi.cpp`
- Create: `src/platform/windows/WindowsHotkeyService.h`
- Create: `src/platform/windows/WindowsHotkeyService.cpp`
- Modify: `tests/WindowsHotkeyTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  `WindowsHotkeyApi::registerHotkey(int, quint32, quint32) -> bool`,
  `WindowsHotkeyApi::unregisterHotkey(int)`,
  `WindowsHotkeyService::registerHotkeys(const ClickProfile&)`
- Consumes: `mapWindowsHotkey`, `QAbstractNativeEventFilter`

- [ ] **Step 1: Write registration and rollback tests**

Use a fake API with registered and unregistered ID lists. Prove all three
profile bindings register on success. Configure failure on the second ID and
prove the first is unregistered, no IDs remain active, and
`registrationFailed` fires exactly once.

- [ ] **Step 2: Write native dispatch tests**

Create a `MSG` with `message = WM_HOTKEY` and `wParam` equal to each service ID.
Call `nativeEventFilter("windows_generic_MSG", &message, &result)` and use
`QSignalSpy` to verify the corresponding `startStopPressed`,
`capturePointPressed`, or `emergencyStopPressed` signal fires once. Unknown IDs
must emit nothing.

- [ ] **Step 3: Run the service tests and verify RED**

```powershell
cmake --build build/windows-msvc-debug --config Debug --target QtClickerWindowsTests
ctest --test-dir build/windows-msvc-debug -C Debug -R WindowsHotkeyService --output-on-failure
```

Expected: FAIL to compile because the API and service are missing.

- [ ] **Step 4: Implement the API and service**

The native adapter wraps:

```cpp
RegisterHotKey(nullptr, id, modifiers, virtualKey);
UnregisterHotKey(nullptr, id);
```

The service installs itself with
`QCoreApplication::instance()->installNativeEventFilter(this)`, owns the active
IDs, unregisters before every new attempt, rolls back on the first failure, and
removes itself from the application in its destructor.

- [ ] **Step 5: Pass service and full Windows tests**

```powershell
cmake --build build/windows-msvc-debug --config Debug --target QtClickerWindowsTests
ctest --test-dir build/windows-msvc-debug -C Debug --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add CMakeLists.txt src/platform/windows/WindowsHotkeyApi.* `
  src/platform/windows/WindowsHotkeyService.* tests/WindowsHotkeyTests.cpp
git commit -m "feat: add transactional Windows global hotkeys"
```

### Task 5: Wire Windows Factories and Platform-Neutral Permission UI

**Files:**
- Create: `src/platform/windows/PlatformServicesWindows.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Create: `tests/MainWindowTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: Windows implementations from `createClickBackend()` and
  `createHotkeyService()`
- Adds a test-only-capable constructor:
  `MainWindow(std::unique_ptr<ClickBackend>, std::unique_ptr<HotkeyService>,
  std::unique_ptr<SettingsRepository>, QWidget*)`

- [ ] **Step 1: Write failing factory and permission-banner tests**

On Windows, assert factory results dynamically identify as
`WindowsClickBackend` and `WindowsHotkeyService`. Construct the window with
fakes, find controls by stable `objectName`, and assert an allowed backend shows
`"输入控制权限：可用"` with the request button hidden.

- [ ] **Step 2: Run tests and verify RED**

```powershell
cmake --build build/windows-msvc-debug --config Debug
ctest --test-dir build/windows-msvc-debug -C Debug -R "PlatformFactory|MainWindow" --output-on-failure
```

Expected: FAIL because the Windows factory and injectable constructor are
missing and the current label is macOS-specific.

- [ ] **Step 3: Implement factories and dependency injection**

The public constructor delegates with a default repository:

```cpp
MainWindow::MainWindow(QWidget* parent)
    : MainWindow(createClickBackend(), createHotkeyService(),
                 std::make_unique<SettingsRepository>(), parent) {}
```

Store the repository as `std::unique_ptr<SettingsRepository>`. The injected
constructor initializes the controller from the supplied backend and lets tests
use a UUID-scoped `QSettings` application name without touching real user
profiles. Assign stable object names to tested widgets. Change the permission copy to
`"输入控制权限：可用"` or `"输入控制权限：需要授权"` and open the macOS settings
URL only under `Q_OS_MACOS`.

- [ ] **Step 4: Pass the focused tests**

Run the Step 2 commands.

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt src/platform/windows/PlatformServicesWindows.cpp `
  src/app/MainWindow.* tests/MainWindowTests.cpp
git commit -m "feat: wire Windows platform services"
```

### Task 6: Complete Cross-Platform Profile Controls

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `tests/MainWindowTests.cpp`

**Interfaces:**
- Consumes/produces: every existing `ClickProfile` field through
  `applyProfileToUi()` and `collectProfileFromUi()`
- Uses existing `SettingsRepository` CRUD methods

- [ ] **Step 1: Write a failing UI round-trip test**

Create a profile containing a right click, fixed point `(640, 480)`, finite
count `8`, jitter `7`, countdown `3`, always-on-top, and Ctrl-modified F6/F7/F8
hotkeys. Apply it to the window and assert `collectProfileFromUi()` returns the
same values.

- [ ] **Step 2: Write failing interaction tests**

Verify target mode enables fixed-coordinate controls, repeat mode enables the
count control, F7 capture copies the fake backend cursor, and running state
disables all configuration controls.

- [ ] **Step 3: Run MainWindow tests and verify RED**

```powershell
cmake --build build/windows-msvc-debug --config Debug --target QtClickerMainWindowTests
ctest --test-dir build/windows-msvc-debug -C Debug -R MainWindow --output-on-failure
```

Expected: FAIL because the controls and current mappings do not exist.

- [ ] **Step 4: Add focused controls and mappings**

Add controls for button, target mode, X/Y coordinates, capture, repeat mode,
repeat count, jitter, countdown, always-on-top, and all three hotkeys. Use enum
values as item data rather than translated display strings. Make
`applyProfileToUi()` and `collectProfileFromUi()` symmetrical.

- [ ] **Step 5: Implement profile CRUD handlers**

Wire new/save/rename/delete/load to the existing repository. Reject blank or
duplicate names with a visible message. Refresh selection after every mutation,
save the last-used profile before starting, and apply `alwaysOnTop` when a
profile is loaded.

- [ ] **Step 6: Extend hotkey validation**

Normalize all three sequences to `PortableText`; reject empty, duplicate,
multi-stroke, or unsupported sequences before registration. Error text names
the offending binding.

- [ ] **Step 7: Pass UI and full tests**

```powershell
cmake --build build/windows-msvc-debug --config Debug
ctest --test-dir build/windows-msvc-debug -C Debug --output-on-failure
```

Expected: all tests PASS.

- [ ] **Step 8: Commit**

```powershell
git add src/app/MainWindow.* tests/MainWindowTests.cpp
git commit -m "feat: expose complete click profiles in the UI"
```

### Task 7: Add Reproducible Windows Packaging and Documentation

**Files:**
- Create: `scripts/package-windows.ps1`
- Modify: `README.md`
- Modify: `.gitignore`

**Interfaces:**
- Produces:
  `scripts/package-windows.ps1 -BuildDir <path> -OutputDir <path> -QtBinDir <path>`
- Consumes: built `QtClicker.exe` and Qt `windeployqt.exe`

- [ ] **Step 1: Write the packaging script contract**

Use mandatory parameters and resolved explicit paths. The script must fail
before changing the output directory if the executable or `windeployqt.exe`
does not exist. It copies the release executable and runs:

```powershell
& $windeployqtPath --release --compiler-runtime --no-translations `
  --dir $resolvedOutputDir $deployedExe
```

Do not discover Qt through global environment variables.

- [ ] **Step 2: Document local Qt configuration**

Document `CMakeUserPresets.json` as an untracked local override and include:

```json
{
  "version": 6,
  "include": ["CMakePresets.json"],
  "configurePresets": [{
    "name": "windows-local",
    "inherits": "windows-msvc-debug",
    "cacheVariables": {
      "CMAKE_PREFIX_PATH": "D:/Qt/6.8.3/msvc2022_64"
    }
  }]
}
```

Add configure, build, CTest, run, and package commands. Add
`CMakeUserPresets.json` and staging directories to `.gitignore`.

- [ ] **Step 3: Verify documentation commands**

```powershell
cmake --preset windows-local
cmake --build --preset windows-build --config Debug
ctest --preset windows-test -C Debug
```

Expected: configure, build, and tests exit `0`.

- [ ] **Step 4: Verify a clean deployment**

Build Release, then run:

```powershell
.\scripts\package-windows.ps1 `
  -BuildDir .\build\windows-msvc-debug `
  -OutputDir .\dist\QtClicker `
  -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin
```

Temporarily launch `dist\QtClicker\QtClicker.exe` by absolute path. Confirm the
window opens without a missing-DLL dialog.

- [ ] **Step 5: Commit**

```powershell
git add .gitignore README.md scripts/package-windows.ps1
git commit -m "docs: add Windows build and packaging workflow"
```

### Task 8: Final Windows Behavioral Verification

**Files:**
- Modify only if a failing verification first produces a regression test

**Interfaces:**
- Verifies all requirements from the approved design

- [ ] **Step 1: Run the fresh automated verification**

```powershell
cmake --preset windows-local
cmake --build --preset windows-build --config Debug
ctest --preset windows-test -C Debug --output-on-failure
```

Expected: all commands exit `0`; CTest reports zero failures.

- [ ] **Step 2: Run manual input verification**

Launch the Debug app and verify in a disposable text/editor window:

1. F6 starts and stops follow-cursor left clicking.
2. F7 captures a fixed point and fixed-point mode clicks it.
3. Right-button mode emits a context click.
4. Jitter remains inside the configured radius.
5. Finite mode stops at the requested count.
6. Countdown delays the first click.
7. F8 stops immediately while the app is unfocused.
8. A conflicting system-wide hotkey produces one error and leaves no partial
   registration.
9. Saved, renamed, loaded, and deleted profiles behave consistently after app
   restart.
10. Always-on-top follows the selected profile.

- [ ] **Step 3: Check repository hygiene**

```powershell
git status --short
git diff --check
git log --oneline -8
```

Expected: only intentionally untracked pre-existing files remain; no whitespace
errors; each task has a focused commit.

- [ ] **Step 4: Record macOS preservation status**

If a macOS runner is available, configure/build/test `macos-debug`. If none is
available, report that macOS was preserved structurally by conditional CMake
selection but was not runtime-verified on this Windows machine; do not claim a
macOS pass.
