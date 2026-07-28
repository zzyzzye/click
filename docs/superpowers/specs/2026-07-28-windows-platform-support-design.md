# Windows Platform Support Design

## Goal

Add a production-quality Windows platform implementation that provides feature
parity with the existing macOS application while preserving the macOS build.
The supported Windows baseline is Windows 10 or Windows 11 on x64, built with
MSVC 2022 and Qt 6.8.3.

## Scope

The Windows application must support the same user-visible behavior as the
current macOS application:

- left- and right-button clicking;
- following the current cursor or clicking a fixed point;
- click-position jitter;
- finite and infinite repeat modes;
- pre-start countdown;
- profile persistence;
- always-on-top mode;
- global start/stop, capture-point, and emergency-stop hotkeys;
- clean configuration, compilation, testing, execution, and deployment on
  Windows.

This work does not add new click modes, keyboard repeating, a new UI design, an
installer, code signing, or elevated-privilege behavior.

## Architecture

The existing `ClickBackend`, `HotkeyService`, and `PlatformServices` boundaries
remain the public platform abstraction. Platform-specific implementation files
live below `src/platform/macos` and `src/platform/windows`; core and UI code must
not include macOS or Windows system headers.

CMake selects exactly one platform implementation:

- macOS enables Objective-C++ and compiles the existing `.mm` sources;
- Windows compiles the Win32 `.cpp` sources and links the required system
  libraries;
- unsupported platforms fail during configuration with a clear message.

The top-level project declares C++ universally and enables Objective-C++ only
inside the Apple branch. This prevents Windows configuration from requiring an
Objective-C++ compiler.

## Windows Click Backend

`WindowsClickBackend` implements `ClickBackend` with Win32 input APIs:

- `GetCursorPos` reads the cursor position;
- fixed-point mode uses the stored screen coordinate;
- jitter is applied independently to both axes using the existing inclusive
  radius behavior;
- `SetCursorPos` positions the cursor at the resolved coordinate;
- `SendInput` emits matching mouse-down and mouse-up events for the selected
  button.

The backend reports failure when cursor positioning or input injection fails.
It must not request elevation or silently retry with broader privileges.
Windows has no macOS-style accessibility consent flow, so the backend reports
input permission as available and treats permission requests as a no-op.

Production calls are placed behind a small Win32 input adapter. The adapter
lets unit tests verify coordinate resolution, button selection, call ordering,
and failure propagation without moving the user's real pointer or generating
real clicks.

## Windows Global Hotkeys

`WindowsHotkeyService` implements `HotkeyService` with `RegisterHotKey` and
`UnregisterHotKey`. It installs a Qt native event filter and translates
`WM_HOTKEY` messages into the existing Qt signals.

Hotkey parsing accepts the same portable `QKeySequence` subset as the macOS
backend:

- F1 through F12;
- A through Z;
- 0 through 9;
- Space, Enter/Return, and Escape;
- Shift, Control, Alt, and Meta modifiers.

Qt Meta maps to the Windows key. Registrations use `MOD_NOREPEAT` when the
running Windows version supports it, preventing key auto-repeat from rapidly
toggling the clicker.

Registration is transactional. Before registering a new profile, the service
removes its previous registrations. If any new hotkey is invalid or already
owned by another application, it removes every registration made during that
attempt, emits one actionable error, and leaves no partial bindings active.
Destruction always unregisters hotkeys and removes the native event filter.

Key-sequence translation is a pure unit-tested component. A small Win32 hotkey
adapter isolates registration calls so rollback, identifier handling, and
failures can be tested without reserving machine-wide hotkeys.

## User Interface Behavior

The UI keeps the current layout and workflow. Permission copy becomes
platform-neutral:

- macOS continues to show whether accessibility permission is granted and
  exposes the button that requests it;
- Windows shows that input control is available and does not show a permission
  request button.

Hotkey registration failures remain visible through the existing warning
mechanism. Error text identifies whether a sequence is unsupported or cannot be
registered because another application likely owns it.

## Build Configuration

The repository gains Windows configure, build, and test presets for Visual
Studio 2022 x64. Repository presets must not contain a developer-specific Qt
installation path.

Qt discovery follows standard CMake mechanisms. A developer may supply
`CMAKE_PREFIX_PATH` on the command line or place a local override in
`CMakeUserPresets.json`, which remains untracked. Documentation includes the
concrete example for `D:\Qt\6.8.3\msvc2022_64` without making that path a
repository requirement.

The Windows executable links only the Win32 libraries required by the selected
APIs. Deployment documentation uses `windeployqt` to produce a runnable folder;
building the application does not depend on Qt being globally added to `PATH`.

## Testing and Verification

Automated tests cover:

- existing core serialization and controller behavior;
- Qt key-sequence to Win32 modifier/key-code translation;
- unsupported and multi-stroke hotkey rejection;
- transactional hotkey-registration rollback;
- dispatch of each registered Windows hotkey action;
- follow-cursor and fixed-point coordinate selection;
- inclusive jitter bounds;
- left- and right-button Win32 input selection;
- Win32 call failure propagation;
- platform factory construction on Windows.

Windows verification consists of:

1. configure with the Visual Studio 2022 x64 preset and the installed Qt path;
2. build both the application and tests;
3. run CTest with failure output enabled;
4. launch the application and verify F6, F7, and F8 from outside the app;
5. verify left/right clicks in follow-cursor and fixed-point modes;
6. run `windeployqt` into a clean staging directory and launch that deployed
   copy without relying on a globally configured Qt `PATH`.

macOS sources and framework linkage remain in their platform branch. The
Windows implementation must not change the public behavior of the existing
core controller or profile format.

## Error Handling

Platform API failures return `false` through existing interfaces and surface
through existing UI feedback where available. Hotkey errors include the
offending portable sequence but never expose unrelated system data. Cleanup is
idempotent so partially initialized services and failed registration attempts
can be destroyed safely.

## Compatibility and Engineering Constraints

- Use public Win32 and Qt 6 APIs; do not depend on private Qt headers.
- Keep native handles and Windows headers out of cross-platform headers where
  practical, using implementation-private types when needed.
- Do not require administrator privileges.
- Do not add third-party runtime dependencies.
- Preserve the existing settings schema and default hotkeys.
- Avoid hard-coded machine paths in committed build files.
- Keep platform behavior behind dependency-injectable adapters where system
  side effects would otherwise make automated tests unsafe.
