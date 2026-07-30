# Windows Macro Recording Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Windows 10/11 system-wide keyboard and mouse macro recording, local persistence, safe window-bound/global playback, visible control hotkeys, and strict mutual exclusion with the existing clicker.

**Architecture:** Keep event models, persistence, compression, scheduling, and automation ownership in platform-neutral core classes. Put low-level hooks, `SendInput`, window enumeration, DPI-aware coordinate conversion, and target validation behind injectable Windows adapters. Add one focused macro page and let `MainWindow` coordinate it with the existing pages and shared status strip.

**Tech Stack:** C++20, Qt 6.8.3 Core/Widgets/Test, CMake 3.24+, MSVC/Visual Studio 2026, Win32 `WH_KEYBOARD_LL`, `WH_MOUSE_LL`, `SendInput`, `RegisterHotKey`, `EnumWindows`, CTest

## Global Constraints

- First-release support is Windows 10 and Windows 11 x64; macOS must continue to compile but macro controls may be disabled there.
- Use public Qt 6 and Win32 APIs only; add no third-party runtime dependency.
- Never request elevation or claim support for protected/anti-cheat applications.
- Keep Win32 headers and `HWND` out of platform-neutral core and UI headers.
- Store macros only under `QStandardPaths::AppDataLocation/macros`; do not upload, export, or log event contents.
- Record visibly and deliberately; show a first-use sensitive-input warning and persistent stop/emergency hotkey copy.
- Preserve complete key/button down-up pairing and release injected input on every stop/error path.
- Click repetition, macro recording, and macro playback are mutually exclusive.
- Preserve all unrelated working-tree edits and stage only files named by each task.

---

## Planned File Structure

- `src/core/MacroTypes.h/.cpp`: platform-neutral macro event, target, playback settings, JSON conversion, and validation.
- `src/core/MacroRepository.h/.cpp`: UUID-named atomic local JSON storage.
- `src/core/AutomationCoordinator.h/.cpp`: exclusive `Idle/Clicking/Recording/Playing` ownership.
- `src/core/MacroCompressor.h/.cpp`: mouse sampling/line compression and control-chord cleanup.
- `src/core/MacroRecorder.h`: asynchronous recording interface and options.
- `src/core/MacroPlayer.h`: single-event injection/preflight/release interface.
- `src/core/MacroController.h/.cpp`: recording collection, absolute-time playback, speed, loops, and cleanup.
- `src/platform/windows/WindowsWindowService.h/.cpp`: visible-window discovery, identity resolution, foreground/size/coordinate checks.
- `src/platform/windows/WindowsMacroRecorder.h/.cpp`: dedicated-thread low-level hooks and event filtering.
- `src/platform/windows/WindowsMacroPlayer.h/.cpp`: `MacroEvent` to `INPUT` conversion and held-input tracking.
- `src/platform/macos/PlatformServicesMac.mm`: return unavailable macro services without changing existing click behavior.
- `src/platform/PlatformServices.h`: factories for recorder, player, and window service.
- `src/app/widgets/WindowPickerButton.h/.cpp`: mouse-grab crosshair target selector.
- `src/app/pages/MacroRecordingPage.h/.cpp`: macro target, library, playback settings, hotkey copy, and controls.
- `src/app/MainWindow.h/.cpp`: wire repositories/controllers/hotkeys/pages and shared exclusivity.
- `src/core/ClickTypes.h/.cpp`: persist F9/F10 macro hotkey defaults with existing profiles.
- `src/core/HotkeyService.h` and Windows/macOS implementations: register and dispatch macro actions.
- `tests/MacroTypesTests.cpp`: model, JSON, repository, compression, and coordinator tests.
- `tests/MacroControllerTests.cpp`: recording/playback timing and cleanup tests with fakes.
- `tests/WindowsMacroTests.cpp`: safe Windows adapter, window, recorder translation, and player conversion tests.
- `tests/ClickFlowPageTests.cpp`, `tests/MainWindowTests.cpp`, `tests/WindowsHotkeyTests.cpp`: UI and integration regressions.
- `CMakeLists.txt`: add source sets, targets, `dwmapi`, and Per-Monitor V2 manifest setting.
- `README.md`: document macro scope, hotkeys, local storage, permissions, and limitations.

### Task 1: Add the Versioned Macro Model and Local Repository

**Files:**
- Create: `src/core/MacroTypes.h`
- Create: `src/core/MacroTypes.cpp`
- Create: `src/core/MacroRepository.h`
- Create: `src/core/MacroRepository.cpp`
- Create: `tests/MacroTypesTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `enum class MacroEventType { KeyDown, KeyUp, MouseMove, MouseButtonDown, MouseButtonUp, Wheel, HorizontalWheel }`.
- Produces `enum class MacroMouseButton { None, Left, Right, Middle, X1, X2 }` and `enum class MacroTargetMode { Global, Window }`.
- Produces `struct WindowTarget { quintptr nativeId; QString executablePath; QString className; QString title; QSize clientSize; }` where `nativeId` is never serialized.
- Produces `struct MacroEvent { MacroEventType type; qint64 offsetUs; quint32 virtualKey; quint32 scanCode; quint32 nativeFlags; QPoint point; MacroMouseButton button; int wheelDelta; }`.
- Produces `struct MacroPlaybackSettings { double speed; bool infinite; int repeatCount; int loopDelayMs; }`.
- Produces `struct MacroSequence { QString id; QString name; QDateTime createdAt; QDateTime modifiedAt; MacroTargetMode targetMode; WindowTarget target; qint64 durationUs; QVector<MacroEvent> events; MacroPlaybackSettings playback; }`.
- Produces `MacroRepository(QString rootPath = {})`, `save`, `loadAll`, `remove`, and `rename` with `QString* error` outputs.

- [ ] **Step 1: Write failing model and repository tests**

Add Qt tests that construct one event of every type, serialize a sequence with a Unicode name and window metadata, deserialize it, and compare every field. Use `QTemporaryDir` to prove save creates `<uuid>.json`, rename changes only the JSON name, delete removes the file, and one malformed file is reported in warnings while valid files still load.

```cpp
MacroSequence sequence;
sequence.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
sequence.name = QStringLiteral("记事本流程");
sequence.targetMode = MacroTargetMode::Window;
sequence.target.executablePath = QStringLiteral("C:/Windows/System32/notepad.exe");
sequence.target.className = QStringLiteral("Notepad");
sequence.target.title = QStringLiteral("无标题 - 记事本");
sequence.target.clientSize = QSize(800, 600);
sequence.durationUs = 240000;
sequence.events = {{MacroEventType::KeyDown, 1200, 'A', 0x1e, 0, {}, MacroMouseButton::None, 0}};
```

- [ ] **Step 2: Run the focused test and verify RED**

```powershell
cmake --preset windows-local
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowMacroTypesTests
```

Expected: configuration or compilation fails because the macro types and target do not exist.

- [ ] **Step 3: Implement version-1 JSON conversion**

Use an integer `schemaVersion` equal to `1`, ISO dates, string enum names, integer microsecond offsets, and JSON arrays for events. Reject missing/unknown versions, empty IDs/names, negative offsets, offsets that decrease, invalid playback values, and a window-mode sequence without executable/class/title identity or positive client size. Keep `nativeId` at zero after load.

```cpp
namespace MacroTypes {
QJsonObject toJson(const MacroSequence& sequence);
std::optional<MacroSequence> fromJson(const QJsonObject& object,
                                      QString* error = nullptr);
bool validate(const MacroSequence& sequence, QString* error = nullptr);
}
```

- [ ] **Step 4: Implement atomic UUID-based storage**

Resolve an empty root to `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/macros"`; create it with `QDir::mkpath`. Use `QSaveFile` and `QJsonDocument::Indented`. Refuse IDs containing path separators and generate a UUID for a new empty ID. Sort `loadAll()` by `modifiedAt` descending and append filename-qualified warnings without event contents.

- [ ] **Step 5: Pass the model tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowMacroTypesTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R MacroTypes --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add CMakeLists.txt src/core/MacroTypes.* src/core/MacroRepository.* tests/MacroTypesTests.cpp
git commit -m "feat: add local macro data model"
```

### Task 2: Add Exclusive Automation Ownership

**Files:**
- Create: `src/core/AutomationCoordinator.h`
- Create: `src/core/AutomationCoordinator.cpp`
- Modify: `src/core/ClickController.h`
- Modify: `src/core/ClickController.cpp`
- Modify: `tests/MacroTypesTests.cpp`
- Modify: `tests/ClickerTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `enum class AutomationActivity { Idle, Clicking, Recording, Playing }`.
- Produces `bool AutomationCoordinator::tryAcquire(AutomationActivity activity, QString* error)`, `void release(AutomationActivity activity)`, `AutomationActivity activity() const`, and `void emergencyStopRequested()` signal.
- `ClickController(ClickBackend*, AutomationCoordinator* = nullptr, QObject* = nullptr)` consumes the coordinator.

- [ ] **Step 1: Write failing exclusivity tests**

Prove idle can acquire each non-idle activity, a second activity gets a Chinese busy error naming the current owner, the wrong owner cannot release, the correct owner returns to idle, and `requestEmergencyStop()` emits once.

```cpp
AutomationCoordinator coordinator;
QString error;
QVERIFY(coordinator.tryAcquire(AutomationActivity::Recording, &error));
QVERIFY(!coordinator.tryAcquire(AutomationActivity::Clicking, &error));
QVERIFY(error.contains(QStringLiteral("录制")));
```

- [ ] **Step 2: Run tests and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowMacroTypesTests QtClickerTests
```

Expected: FAIL because `AutomationCoordinator` is missing.

- [ ] **Step 3: Implement the coordinator and integrate the clicker**

`tryAcquire(Idle)` always fails. `release` changes state only when its argument equals the current state. In `ClickController::start`, acquire `Clicking` after profile validation and before setting `running_`; if acquisition fails emit `startRejected`. In every `finishRun` path release `Clicking`. Preserve old tests by allowing a null coordinator.

- [ ] **Step 4: Pass focused and regression tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowMacroTypesTests QtClickerTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R "MacroTypes|QtClickerTests" --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt src/core/AutomationCoordinator.* src/core/ClickController.* tests/MacroTypesTests.cpp tests/ClickerTests.cpp
git commit -m "feat: coordinate exclusive automation activity"
```

### Task 3: Extend Global Hotkeys for Macro Control

**Files:**
- Modify: `src/core/ClickTypes.h`
- Modify: `src/core/ClickTypes.cpp`
- Modify: `src/core/HotkeyService.h`
- Modify: `src/platform/windows/WindowsHotkeyService.h`
- Modify: `src/platform/windows/WindowsHotkeyService.cpp`
- Modify: `src/platform/macos/MacOSHotkeyService.h`
- Modify: `src/platform/macos/MacOSHotkeyService.mm`
- Modify: `src/app/pages/HotkeySettingsPage.h`
- Modify: `src/app/pages/HotkeySettingsPage.cpp`
- Modify: `tests/ClickerTests.cpp`
- Modify: `tests/WindowsHotkeyTests.cpp`
- Modify: `tests/ClickFlowPageTests.cpp`

**Interfaces:**
- Extends `HotkeyBindings` with `QString macroRecord = "F9"` and `QString macroPlayback = "F10"`.
- Adds `HotkeyService::macroRecordPressed()` and `macroPlaybackPressed()` signals.
- Windows action IDs remain stable: click `1`, capture `2`, emergency `3`, record `4`, playback `5`.

- [ ] **Step 1: Write failing persistence, registration, and page-label tests**

Prove old maps default to F9/F10, new maps round-trip custom bindings, Windows registers five IDs transactionally, `WM_HOTKEY` IDs 4/5 emit the new signals, and `HotkeySettingsPage` contains visible labels “宏录制” and “宏回放”. Add a validation test that any duplicate among all five bindings fails.

- [ ] **Step 2: Run focused tests and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target QtClickerTests QtClickerWindowsHotkeyTests ClickFlowPageTests
```

Expected: FAIL because the fields, signals, and controls are missing.

- [ ] **Step 3: Implement five-binding persistence and validation**

Serialize as `macroRecordHotkey` and `macroPlaybackHotkey`. Add two `QKeySequenceEdit` rows with object names `macroRecordHotkeyEdit` and `macroPlaybackHotkeyEdit`. Normalize all values using `PortableText`, reject empty/multi-stroke/duplicates, and keep the repeated keyboard-key collision check against all five main keys.

- [ ] **Step 4: Register and dispatch the new Windows actions**

Append the two bindings to the existing transactional array and cover the new enum cases in `dispatch`. Update rollback expectations so a failure at ID 5 unregisters IDs 1 through 4.

- [ ] **Step 5: Preserve macOS compilation**

Add matching Carbon action IDs/signals and registrations using the existing mapping/rollback pattern. This is hotkey registration compatibility only; the macro page remains unavailable on macOS.

- [ ] **Step 6: Pass focused tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target QtClickerTests QtClickerWindowsHotkeyTests ClickFlowPageTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R "QtClickerTests|WindowsHotkeyMapping|ClickFlowPages" --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```powershell
git add src/core/ClickTypes.* src/core/HotkeyService.h src/platform/windows/WindowsHotkeyService.* src/platform/macos/MacOSHotkeyService.* src/app/pages/HotkeySettingsPage.* tests/ClickerTests.cpp tests/WindowsHotkeyTests.cpp tests/ClickFlowPageTests.cpp
git commit -m "feat: add visible macro control hotkeys"
```

### Task 4: Implement Window Discovery and Safe Target Resolution

**Files:**
- Create: `src/core/WindowService.h`
- Create: `src/platform/windows/WindowsWindowApi.h`
- Create: `src/platform/windows/WindowsWindowApi.cpp`
- Create: `src/platform/windows/WindowsWindowService.h`
- Create: `src/platform/windows/WindowsWindowService.cpp`
- Create: `tests/WindowsMacroTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `WindowService::availableWindows()`, `windowAt(const QPoint&)`, `resolve(const WindowTarget&)`, `isAlive`, `isForeground`, `activate`, `clientSize`, `screenToClient`, `clientToScreen`, and `virtualDesktopRect`.
- Produces `bool WindowService::sizeMatches(const QSize& recorded, const QSize& current)` with tolerance `max(8 px, 2%)` on each dimension.

- [ ] **Step 1: Write failing pure behavior tests with a fake API**

Provide visible, hidden, cloaked, untitled, and ClickFlow windows. Verify only eligible top-level windows remain and display text is `title + " — " + executable file name`. Prove exact identity resolution, ambiguous matches return no target, crosshair child handles normalize to root, and size differences at/below tolerance pass while larger differences fail.

- [ ] **Step 2: Run the Windows macro test and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowWindowsMacroTests
```

Expected: FAIL because the window service does not exist.

- [ ] **Step 3: Implement the injectable Win32 boundary**

Wrap `EnumWindows`, `IsWindowVisible`, `GetWindowTextW`, `GetClassNameW`, `GetWindowThreadProcessId`, `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)`, `QueryFullProcessImageNameW`, `DwmGetWindowAttribute(DWMWA_CLOAKED)`, `GetAncestor(GA_ROOT)`, `WindowFromPoint`, `GetClientRect`, `ClientToScreen`, `ScreenToClient`, `GetForegroundWindow`, `SetForegroundWindow`, `IsWindow`, and virtual-screen metrics. Never retain a process handle after each query.

- [ ] **Step 4: Implement deterministic identity resolution and coordinates**

Prefer exact executable path + class + title, then executable path + class when unique. Exclude the supplied ClickFlow native ID. Return a user-facing error for zero or multiple candidates. Keep `HWND` converted to/from `quintptr` only inside `.cpp` files.

- [ ] **Step 5: Pass tests and link DWM**

Link `dwmapi` only on Windows, then run:

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowWindowsMacroTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R WindowsMacro --output-on-failure
```

Expected: PASS without enumerating or activating real windows in tests.

- [ ] **Step 6: Commit**

```powershell
git add CMakeLists.txt src/core/WindowService.h src/platform/windows/WindowsWindowApi.* src/platform/windows/WindowsWindowService.* tests/WindowsMacroTests.cpp
git commit -m "feat: discover safe Windows macro targets"
```

### Task 5: Implement Low-Level Recording and Trajectory Compression

**Files:**
- Create: `src/core/MacroRecorder.h`
- Create: `src/core/MacroCompressor.h`
- Create: `src/core/MacroCompressor.cpp`
- Create: `src/platform/windows/WindowsMacroHookApi.h`
- Create: `src/platform/windows/WindowsMacroHookApi.cpp`
- Create: `src/platform/windows/WindowsMacroRecorder.h`
- Create: `src/platform/windows/WindowsMacroRecorder.cpp`
- Modify: `tests/MacroTypesTests.cpp`
- Modify: `tests/WindowsMacroTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `struct MacroRecordingOptions { MacroTargetMode targetMode; WindowTarget target; QStringList reservedHotkeys; }`.
- Produces QObject interface `MacroRecorder::start(const MacroRecordingOptions&, QString*)`, `stop()`, `isRecording()`, signals `eventCaptured(MacroEvent)`, `targetLost(QString)`, and `recordingFailed(QString)`.
- Produces `QVector<MacroEvent> MacroCompressor::compress(const QVector<MacroEvent>&)` and `removeReservedTail(..., const QStringList&)`.

- [ ] **Step 1: Write failing compression and pairing tests**

Use movements at 1 ms intervals to prove sampling keeps at most one point per 16 ms unless a mouse button transition occurs. Prove nearly collinear points within one physical pixel collapse, drag paths retain their endpoints, every button/key transition remains ordered, and a terminal `Ctrl+F9` chord is removed with both Ctrl and F9 down/up events.

- [ ] **Step 2: Write failing recorder translation tests**

Feed fake `KBDLLHOOKSTRUCT`/`MSLLHOOKSTRUCT` equivalents through the injectable API. Verify scan code, virtual key, extended flag, horizontal/vertical wheel, X buttons, timestamps, and global/window coordinates. Verify events with the ClickFlow `dwExtraInfo` marker are ignored. In window mode, ignore keyboard input while another window is foreground and continue an in-target drag until button-up outside.

- [ ] **Step 3: Run tests and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowMacroTypesTests ClickFlowWindowsMacroTests
```

Expected: FAIL because recorder/compressor interfaces are missing.

- [ ] **Step 4: Implement the hook thread**

Start a dedicated `QThread`; install both low-level hooks on that thread and run its event loop. The static callbacks copy only primitive fields plus `GetMessageTime`/monotonic offset into a queued signal. Unhook both handles on the same thread before stopping. If either hook fails, remove the other and report one error.

Use a shared marker constant in a Windows-only header:

```cpp
inline constexpr ULONG_PTR kClickFlowInjectedInputMarker =
    sizeof(ULONG_PTR) == 8 ? 0x43464C4F574D4143ULL : 0x574D4143UL;
```

- [ ] **Step 5: Implement target filtering and compression**

Convert window-mode mouse points with `WindowService::screenToClient`. Track a bitmask of pressed mouse buttons so an in-target drag remains active outside the client rect. Buffer terminal modifier events long enough to remove a matched reserved control chord without orphaning modifiers. Keep all non-move event offsets unchanged and set sequence duration from the last retained event.

- [ ] **Step 6: Pass focused tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowMacroTypesTests ClickFlowWindowsMacroTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R "MacroTypes|WindowsMacro" --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/core/MacroRecorder.h src/core/MacroCompressor.* src/platform/windows/WindowsMacroHookApi.* src/platform/windows/WindowsMacroRecorder.* tests/MacroTypesTests.cpp tests/WindowsMacroTests.cpp
git commit -m "feat: record and compress Windows input"
```

### Task 6: Implement Safe Input Injection and Release Tracking

**Files:**
- Create: `src/core/MacroPlayer.h`
- Create: `src/platform/windows/WindowsMacroInputApi.h`
- Create: `src/platform/windows/WindowsMacroInputApi.cpp`
- Create: `src/platform/windows/WindowsMacroPlayer.h`
- Create: `src/platform/windows/WindowsMacroPlayer.cpp`
- Modify: `tests/WindowsMacroTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `struct MacroPlaybackContext { MacroTargetMode targetMode; WindowTarget target; }`.
- Produces QObject interface `MacroPlayer::prepare(const MacroSequence&, QString*)`, `inject(const MacroEvent&, QString*)`, `releaseAll()`, and `cancel()`.

- [ ] **Step 1: Write failing event-conversion and cleanup tests**

Prove key events use `KEYEVENTF_SCANCODE`, `KEYEVENTF_KEYUP`, and `KEYEVENTF_EXTENDEDKEY` as appropriate; mouse movement uses absolute virtual-desktop coordinates with `MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK`; buttons/wheels map correctly; every input carries the marker. Simulate stopping after key-down and left-down and assert `releaseAll()` injects exactly their matching up events once.

- [ ] **Step 2: Write failing preflight tests**

For window mode, reject dead/unresolved targets, failed foreground activation, and size mismatch. Convert client-relative coordinates at injection time so moving a same-sized target changes screen coordinates correctly. For global mode, reject any recorded point outside the current virtual desktop.

- [ ] **Step 3: Run and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowWindowsMacroTests
```

Expected: FAIL because the player/input API is missing.

- [ ] **Step 4: Implement `SendInput` conversion**

Normalize absolute coordinates with `(coordinate - virtualOrigin) * 65535 / max(1, extent - 1)`. Send one `INPUT` per event so partial failures identify the exact sequence position. Return a UIPI-oriented Chinese error when `SendInput` returns zero and include only `GetLastError()` numeric context, never event contents.

- [ ] **Step 5: Implement held-input tracking**

Track keys by scan code plus extended bit and mouse buttons by enum. Remove them on matching up events. `releaseAll()` emits mouse ups and key ups in reverse press order, clears tracking even if release injection fails, and is idempotent.

- [ ] **Step 6: Pass Windows macro tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowWindowsMacroTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R WindowsMacro --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/core/MacroPlayer.h src/platform/windows/WindowsMacroInputApi.* src/platform/windows/WindowsMacroPlayer.* tests/WindowsMacroTests.cpp
git commit -m "feat: safely replay Windows macro events"
```

### Task 7: Build the Recording and Absolute-Time Playback Controller

**Files:**
- Create: `src/core/MacroController.h`
- Create: `src/core/MacroController.cpp`
- Create: `tests/MacroControllerTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `MacroController(MacroRecorder*, MacroPlayer*, AutomationCoordinator*, QObject* = nullptr)`.
- Produces `startRecording`, `stopRecording`, `startPlayback`, `stop`, `emergencyStop`, `isRecording`, `isPlaying`, and `currentSequence`.
- Emits `stateChanged`, `recordingProgress(qint64,int)`, `recordingCompleted(MacroSequence)`, `playbackProgress(int,int)`, `statusChanged(QString)`, and `failed(QString)`.

- [ ] **Step 1: Write failing recording lifecycle tests with fakes**

Prove recording acquires `Recording`, passes target/reserved hotkeys, appends emitted events, compresses on stop, releases ownership, and emits one completed sequence. Hook failure and target loss release ownership without emitting a saved empty sequence.

- [ ] **Step 2: Write failing deterministic playback tests**

Inject a fake monotonic clock and timer scheduler. For events at 0, 100000, and 250000 microseconds, prove 2x targets are 0, 50000, and 125000; late callbacks schedule from the absolute round start instead of accumulating drift. Prove finite loops, infinite loops, loop delay, ordinary stop, emergency stop, injection failure, and `releaseAll()` behavior.

- [ ] **Step 3: Run and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowMacroControllerTests
```

Expected: FAIL because `MacroController` is missing.

- [ ] **Step 4: Implement state transitions and recording**

Use an internal `Idle/Recording/Playing` state. Validate speed against `{0.5, 1.0, 1.5, 2.0}`, finite repeat count `>= 1`, and loop delay `>= 0`. Connect recorder signals with queued connections. On stop, remove the reserved terminal chord, compress, validate, and emit the sequence for repository saving.

- [ ] **Step 5: Implement drift-corrected playback**

Use `QElapsedTimer` and a single-shot `QTimer` with `Qt::PreciseTimer`. Calculate each deadline from `roundStartUs + event.offsetUs / speed`. Dispatch all events whose deadlines are already due, then schedule the next positive remaining duration. Between rounds use the configured delay, reset the absolute round start, and never recursively inject an unbounded event burst.

- [ ] **Step 6: Pass controller and core tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowMacroControllerTests ClickFlowMacroTypesTests QtClickerTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R "MacroController|MacroTypes|QtClickerTests" --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/core/MacroController.* tests/MacroControllerTests.cpp
git commit -m "feat: control macro recording and playback"
```

### Task 8: Build the Macro Page and Crosshair Window Picker

**Files:**
- Create: `src/app/widgets/WindowPickerButton.h`
- Create: `src/app/widgets/WindowPickerButton.cpp`
- Create: `src/app/pages/MacroRecordingPage.h`
- Create: `src/app/pages/MacroRecordingPage.cpp`
- Modify: `src/app/widgets/NavigationSidebar.h`
- Modify: `src/app/widgets/NavigationSidebar.cpp`
- Modify: `tests/ProductShellTests.cpp`
- Modify: `tests/ClickFlowPageTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Adds `ShellPage::MacroRecording = 1` and shifts Hotkeys/Presets to 2/3.
- `WindowPickerButton` emits `windowPointSelected(QPoint globalPoint)` after a press-drag-release mouse grab.
- `MacroRecordingPage` emits `recordRequested(MacroRecordingOptions)`, `stopRequested`, `playRequested(QString id, MacroPlaybackSettings)`, `deleteRequested`, `renameRequested`, `refreshWindowsRequested`, and `windowPointSelected`.
- Provides setters for macro list, window list, hotkey text/status, activity state, selection details, and errors.

- [ ] **Step 1: Write failing navigation and page-contract tests**

Assert sidebar/page count is four and labels appear in order. Construct the page offscreen and verify object names `macroRecordHotkeyLabel`, `macroPlaybackHotkeyLabel`, `macroEmergencyHotkeyLabel`, `macroTargetModeCombo`, `macroWindowCombo`, `macroRecordButton`, `macroPlayButton`, `macroSpeedCombo`, `macroRepeatModeCombo`, `macroRepeatCountSpin`, and `macroLoopDelaySpin`.

- [ ] **Step 2: Write failing behavior tests**

Prove target window controls enable only in window mode; finite count enables only for finite mode; recording disables playback/library mutation; playing disables recording/library mutation; all three hotkey labels remain visible in every state; and the buttons emit fully populated options/settings.

- [ ] **Step 3: Run and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowProductShellTests ClickFlowPageTests
```

Expected: FAIL because the page and fourth navigation item do not exist.

- [ ] **Step 4: Implement focused cards and controls**

Use existing `settingsCard`/`cardTitle` styling. Add cards for hotkeys, target, recordings, and playback. Display hotkeys as non-editable prominent labels such as `录制：F9`, `回放：F10`, `紧急停止：F8`; edits remain on the Hotkeys page. Speeds store numeric item data `{0.5,1.0,1.5,2.0}`. Use `QComboBox` for saved macros and windows, `QSpinBox` bounds `1..999999` repeats and `0..3600000 ms` loop delay.

- [ ] **Step 5: Implement the crosshair button**

On left press call `grabMouse()`, set `Qt::CrossCursor`, and show “释放以选择窗口”. On release capture `QCursor::pos()`, restore cursor, release mouse, restore label, and emit. Handle focus loss/cancel by releasing the grab without emitting.

- [ ] **Step 6: Pass page tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target ClickFlowProductShellTests ClickFlowPageTests
ctest --test-dir build/windows-vs2026-debug -C Debug -R "ProductShell|ClickFlowPages" --output-on-failure
```

Expected: PASS.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/app/widgets/WindowPickerButton.* src/app/pages/MacroRecordingPage.* src/app/widgets/NavigationSidebar.* tests/ProductShellTests.cpp tests/ClickFlowPageTests.cpp
git commit -m "feat: add macro recording workspace"
```

### Task 9: Integrate Services, Persistence, Hotkeys, and Main Window State

**Files:**
- Modify: `src/platform/PlatformServices.h`
- Modify: `src/platform/windows/PlatformServicesWindows.cpp`
- Modify: `src/platform/macos/PlatformServicesMac.mm`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/core/SettingsRepository.h`
- Modify: `src/core/SettingsRepository.cpp`
- Modify: `src/app/widgets/StatusStrip.h`
- Modify: `src/app/widgets/StatusStrip.cpp`
- Modify: `tests/MainWindowTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Adds factories `createMacroRecorder(WindowService*)`, `createMacroPlayer(WindowService*)`, and `createWindowService()`; macOS returns null services.
- Adds `SettingsRepository::macroSafetyAcknowledged()` and `setMacroSafetyAcknowledged(bool)`.
- MainWindow test constructor accepts optional injected recorder/player/window service/macro repository while preserving the existing three-argument call sites through defaults or a delegating overload.

- [ ] **Step 1: Write failing integration tests**

Using fakes and a temporary macro directory, verify: the macro page is the second scroll page; Windows factories return native services; macro hotkeys trigger record/play; F8 stops the active click/record/play owner; starting click while recording is rejected; completed recording is saved after a supplied test name; page selection hides the click-only `ActionBar` on the macro page and restores it elsewhere; status text always names the active stop and emergency hotkeys.

- [ ] **Step 2: Write the first-use warning test**

With acknowledgment false, a record request shows copy containing “密码” and records only after acceptance; acceptance persists. With acknowledgment true, no dialog appears. Tests must use an injectable confirmation callback rather than interacting with a native message box.

- [ ] **Step 3: Run and verify RED**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --target QtClickerMainWindowTests
```

Expected: FAIL because factories and integration wiring are absent.

- [ ] **Step 4: Add platform factories and application ownership**

Construct one `AutomationCoordinator`, pass it to `ClickController` and `MacroController`, and own services before controllers so destruction order stops controllers first. On Windows create native window/recorder/player services. On macOS show “键鼠录制当前仅支持 Windows 10/11” and disable page actions.

- [ ] **Step 5: Wire page actions and local repository**

Refresh window choices on demand, resolve crosshair points, prefill saved macros, prompt for a nonblank unique name after recording, and call repository rename/delete with visible errors. Hide the persistent click `ActionBar` only while the macro page is selected because the macro page owns its record/play buttons.

- [ ] **Step 6: Wire all hotkeys and status copy**

F9 toggles recording, F10 toggles selected macro playback, and F8 calls both controllers' emergency-stop paths through the coordinator. When recording show `录制中 · F9 停止 · F8 紧急停止`; when playing show `回放中 · F10 停止 · F8 紧急停止`; otherwise show idle/progress text. Re-register all five bindings transactionally whenever the profile hotkey settings change or a run begins.

- [ ] **Step 7: Enable Per-Monitor V2 DPI and build sources**

Set `WIN32_EXECUTABLE TRUE` and add a Windows manifest dependency with `PerMonitorV2` DPI awareness, or add equivalent XML to `ClickFlow.rc.in`. Add every new source to core/app/Windows test targets and link `dwmapi` with `user32`.

- [ ] **Step 8: Pass integration and full automated tests**

```powershell
cmake --build build/windows-vs2026-debug --config Debug --parallel
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

Expected: all tests PASS, including existing clicker, identity, shell, page, Windows backend, hotkey, and MainWindow tests.

- [ ] **Step 9: Commit**

```powershell
git add CMakeLists.txt src/platform/PlatformServices.h src/platform/windows/PlatformServicesWindows.cpp src/platform/macos/PlatformServicesMac.mm src/app/MainWindow.* src/core/SettingsRepository.* src/app/widgets/StatusStrip.* tests/MainWindowTests.cpp src/app/ClickFlow.rc.in
git commit -m "feat: integrate Windows macro automation"
```

### Task 10: Document and Verify the Complete Windows Workflow

**Files:**
- Modify: `README.md`
- Modify only on test-discovered defects: source/test files from Tasks 1-9

**Interfaces:**
- Verifies every acceptance criterion in `docs/superpowers/specs/2026-07-30-windows-macro-recording-design.md`.

- [ ] **Step 1: Document user-visible behavior and limitations**

Add sections for global/window modes, F9/F10/F8 defaults, visible hotkey editing, speed/loop settings, local macro directory semantics, target size protection, administrator/UIPI limits, sensitive-input warning, and unsupported protected games. Do not include real local data paths containing usernames.

- [ ] **Step 2: Run a fresh configure, build, and test**

```powershell
cmake --preset windows-local
cmake --build build/windows-vs2026-debug --config Debug --parallel
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

Expected: all commands exit `0` and CTest reports zero failures.

- [ ] **Step 3: Perform safe manual recording checks**

Launch the Debug application and use disposable Notepad content only. Verify key down/up, modifiers, mouse movement, left/right/middle clicks, vertical/horizontal wheel, drag, all four speeds, finite/infinite loops, round delay, F9/F10/F8, and no stuck input after mid-drag/mid-key cancellation.

- [ ] **Step 4: Perform target and failure checks**

Verify both window list and crosshair selection, same-sized moved-window replay, size mismatch rejection, target close/foreground loss stop, multi-monitor coordinates, click/record/play exclusivity, non-recursion of injected events, corrupt JSON isolation, restart persistence, and clear failure against an elevated disposable application.

- [ ] **Step 5: Check repository hygiene**

```powershell
git diff --check
git status --short
git log --oneline -12
```

Expected: no whitespace errors; only intentional source changes plus pre-existing user edits; no `.env`, macro data, generated build output, credentials, or event contents staged.

- [ ] **Step 6: Commit documentation**

```powershell
git add README.md
git commit -m "docs: explain Windows macro recording"
```
