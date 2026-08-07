# Smooth Page Scrolling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make ClickFlow setting pages react to mouse wheels and touchpads with direct, continuous smooth vertical scrolling.

**Architecture:** Add a focused `SmoothScrollArea` widget that owns a short `QPropertyAnimation` for its vertical scroll bar and handles wheel input at the viewport. Replace the four generic page containers in `MainWindow` with this widget while preserving all existing scroll-area policies and form-control wheel protection.

**Tech Stack:** C++20, Qt 6 Widgets, Qt Test, CMake/CTest.

## Global Constraints

- Scope is limited to the four vertical settings-page scroll areas in the main window.
- Do not alter page layout, content height, scrollbar visual styling, or application colour system.
- `QComboBox` and `QAbstractSpinBox` must keep ignoring wheel input to prevent accidental setting changes.
- Mouse-wheel and touchpad movement must not be quantized to a fixed row or pixel step.
- Animation duration is 160ms with an easing-out curve; fresh wheel input replaces the active target immediately.
- Horizontal wheel input and non-wheel navigation retain Qt's default behaviour.
- The scroll target must always remain inside the vertical scroll bar's minimum/maximum range.

---

## File Structure

- Create `src/app/widgets/SmoothScrollArea.h`: declares the reusable vertical scroll container and its wheel-event interface.
- Create `src/app/widgets/SmoothScrollArea.cpp`: translates wheel deltas into constrained scroll targets and animates the vertical bar.
- Modify `src/app/MainWindow.cpp`: constructs `SmoothScrollArea` for each setting page instead of `QScrollArea`.
- Modify `CMakeLists.txt`: adds the widget source and header to the application and main-window test target.
- Modify `tests/MainWindowTests.cpp`: verifies shell integration and smooth-scroll behaviour.

### Task 1: Add a testable smooth scroll container

**Files:**
- Create: `src/app/widgets/SmoothScrollArea.h`
- Create: `src/app/widgets/SmoothScrollArea.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/MainWindowTests.cpp`

**Interfaces:**
- Consumes: `QScrollArea`, `QWheelEvent`, `QPropertyAnimation`, and the existing Qt Test executable structure.
- Produces: `class SmoothScrollArea final : public QScrollArea` with `explicit SmoothScrollArea(QWidget* parent = nullptr)` and protected `void wheelEvent(QWheelEvent* event) override`.

- [ ] **Step 1: Write the failing tests**

  Add the widget include, `QScrollBar`, `QWheelEvent`, and `QSignalSpy` includes to `tests/MainWindowTests.cpp`. Add these slots to `MainWindowTests`:

  ```cpp
  void smoothScrollUsesContinuousWheelTarget();
  void smoothScrollClampsAtBoundaries();
  ```

  Create a `SmoothScrollArea`, set a 1000px-high child widget and a 200px viewport, then dispatch a vertical wheel event to its viewport. Assert the event is accepted, the vertical bar value has advanced after a short wait, and it reaches a non-step-aligned target such as 37px. Dispatch a second event before the first animation settles and assert the final target incorporates both deltas rather than waiting for a second queued animation. Use `QTRY_COMPARE_WITH_TIMEOUT` with a 500ms timeout.

  For the boundary test, set the bar to its minimum, dispatch an upward wheel event, and assert its final value stays at `minimum()`. Set it to `maximum()`, dispatch a downward event, and assert its final value stays at `maximum()`.

- [ ] **Step 2: Run the new test to verify it fails**

  Run:

  ```powershell
  cmake --build build --target QtClickerMainWindowTests
  ctest --test-dir build -R MainWindow --output-on-failure
  ```

  Expected: compilation fails because `app/widgets/SmoothScrollArea.h` does not exist.

- [ ] **Step 3: Implement the minimal container**

  Create `SmoothScrollArea` with a child-owned `QPropertyAnimation` targeting `verticalScrollBar()` and `"value"`. In `wheelEvent`, pass through horizontal-dominant or zero vertical input to `QScrollArea::wheelEvent(event)`. For vertical movement, use `pixelDelta().y()` when nonzero; otherwise convert `angleDelta().y()` proportionally to 37 logical pixels per 120 angle units. Read the current animated value when running, add the signed delta, clamp with `std::clamp` to the scroll bar range, stop the active animation, set start and end values, run a 160ms `QEasingCurve::OutCubic` animation, and accept the event. Do nothing beyond accepting when the constrained target equals the current display value.

  Use a private `int targetValue_` only if needed to retain the latest target across event bursts; never round wheel movement to a line-step multiple.

  Add the new source/header to `SHELL_WIDGET_SOURCES`, which is already used by the application and `ClickFlowProductShellTests`, and add those same files explicitly to `QtClickerMainWindowTests` through that variable.

- [ ] **Step 4: Run the targeted tests to verify they pass**

  Run:

  ```powershell
  cmake --build build --target QtClickerMainWindowTests
  ctest --test-dir build -R MainWindow --output-on-failure
  ```

  Expected: `MainWindow` passes, including both new smooth-scroll tests.

- [ ] **Step 5: Commit the tested widget**

  ```powershell
  git add CMakeLists.txt src/app/widgets/SmoothScrollArea.h src/app/widgets/SmoothScrollArea.cpp tests/MainWindowTests.cpp
  git commit -m "feat: add smooth page scrolling"
  ```

### Task 2: Integrate smooth scrolling with every settings page

**Files:**
- Modify: `src/app/MainWindow.cpp:12,255-263`
- Modify: `tests/MainWindowTests.cpp:307-313`

**Interfaces:**
- Consumes: `SmoothScrollArea(QWidget*)` from Task 1.
- Produces: all four widgets in `contentPages` are `SmoothScrollArea` instances with their existing resize and scrollbar policies unchanged.

- [ ] **Step 1: Write the failing integration assertion**

  Add `#include "app/widgets/SmoothScrollArea.h"` to `tests/MainWindowTests.cpp`. In `usesPersistentClickFlowShell`, replace the `qobject_cast<QScrollArea*>` assertion with a `qobject_cast<SmoothScrollArea*>` assertion, retaining assertions for `widgetResizable()` and `Qt::ScrollBarAlwaysOff` horizontally.

- [ ] **Step 2: Run the assertion to verify it fails**

  Run:

  ```powershell
  cmake --build build --target QtClickerMainWindowTests
  ctest --test-dir build -R MainWindow --output-on-failure
  ```

  Expected: `usesPersistentClickFlowShell` fails because `MainWindow` still creates a generic `QScrollArea`.

- [ ] **Step 3: Wire the component into the main window**

  Replace the `#include <QScrollArea>` include in `src/app/MainWindow.cpp` with `#include "app/widgets/SmoothScrollArea.h"`. In `addScrollablePage`, replace `new QScrollArea(pages_)` with `new SmoothScrollArea(pages_)`; retain calls to `setWidgetResizable(true)`, `setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff)`, `setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded)`, `setFrameShape(QFrame::NoFrame)`, and `setWidget(page)` unchanged.

- [ ] **Step 4: Run all relevant tests**

  Run:

  ```powershell
  cmake --build build --target QtClickerMainWindowTests ClickFlowPageTests ClickFlowProductShellTests
  ctest --test-dir build -R "MainWindow|ClickFlowPages|ClickFlowProductShell" --output-on-failure
  ```

  Expected: all three named tests pass.

- [ ] **Step 5: Commit integration**

  ```powershell
  git add src/app/MainWindow.cpp tests/MainWindowTests.cpp
  git commit -m "feat: apply smooth scrolling to settings pages"
  ```

### Task 3: Verify the full project and desktop interaction

**Files:**
- Modify: no source files expected.

**Interfaces:**
- Consumes: the completed `SmoothScrollArea` integration from Tasks 1 and 2.
- Produces: verified build and regression evidence.

- [ ] **Step 1: Configure and run the complete automated suite**

  Run:

  ```powershell
  cmake --preset windows-debug
  cmake --build --preset windows-debug
  ctest --preset windows-debug --output-on-failure
  ```

  Expected: the configured build and every registered test pass.

- [ ] **Step 2: Manually verify native interaction**

  Launch the Windows debug application. On every settings page that overflows vertically, test an ordinary mouse wheel and a precision touchpad. Confirm movement is continuous rather than fixed-step, repeated inputs stay responsive, and stopping input settles within roughly 160ms. Hover each combo box and spin box while scrolling to confirm their values do not change. Drag the vertical scrollbar thumb and use keyboard Page Up/Page Down to confirm native operation remains intact.

- [ ] **Step 3: Record the verification result**

  If the manual check exposes a defect, add a focused regression test before changing production code, rerun the full suite, and make a corrective commit. If it does not, make no further source edits.
