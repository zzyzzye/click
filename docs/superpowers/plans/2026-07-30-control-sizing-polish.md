# ClickFlow Control Sizing Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make ClickFlow buttons and form controls visually consistent, with 40px standard controls and 44px primary actions.

**Architecture:** Extract `MainWindow`'s application-level Qt Style Sheet into a small `clickFlowStyleSheet()` function so the style remains the single source of control sizing and can be tested on macOS. Use existing object names to give the three primary action buttons a 44px treatment, while removing the macro page's local button sizing overrides.

**Tech Stack:** C++20, Qt 6 Widgets, Qt Style Sheets, Qt Test, CMake, Ninja

## Global Constraints

- Ordinary buttons and form input controls have a visual height of exactly 40px.
- Primary action buttons have a visual height of exactly 44px.
- Ordinary button horizontal padding is 16px; primary action horizontal padding is 22px.
- Ordinary controls use an 8px corner radius; primary actions use a 9px corner radius.
- Preserve the existing page structure, window dimensions, functionality, and blue-gray color direction.
- Do not add per-widget `setFixedHeight` calls; application-level style rules remain the single source of sizing.
- Preserve existing horizontal-scroll and vertical-scroll behavior.

---

### Task 1: Unify control sizing and interaction states

**Files:**
- Create: `src/app/UiStyle.h`
- Create: `src/app/UiStyle.cpp`
- Modify: `tests/ClickFlowPageTests.cpp`
- Modify: `CMakeLists.txt:69-73,278-286`
- Modify: `src/app/MainWindow.cpp:260-328`
- Modify: `src/app/pages/MacroRecordingPage.cpp:163-179`

**Interfaces:**
- Consumes: Existing widget object names `startStopButton`, `macroRecordButton`, and `macroPlayButton`.
- Produces: `QString clickFlowStyleSheet()` and application-level rules that give ordinary controls a 40px `sizeHint().height()` and primary actions a 44px `sizeHint().height()`.

- [ ] **Step 1: Declare the new UI test**

Add these includes to `tests/ClickFlowPageTests.cpp`:

```cpp
#include <QHBoxLayout>
#include <QSpinBox>
#include <QWidget>
```

Forward-declare the wished-for style interface above the test class:

```cpp
QString clickFlowStyleSheet();
```

Add this slot to `ClickFlowPageTests`:

```cpp
void controlsUseConsistentComfortableHeights();
```

- [ ] **Step 2: Write the failing height test**

Add this test to `tests/ClickFlowPageTests.cpp`:

```cpp
void ClickFlowPageTests::controlsUseConsistentComfortableHeights() {
  QWidget host;
  host.setStyleSheet(clickFlowStyleSheet());
  auto* layout = new QHBoxLayout(&host);

  auto* standardButton = new QPushButton("普通操作", &host);
  auto* combo = new QComboBox(&host);
  combo->addItem("选项");
  auto* spinBox = new QSpinBox(&host);
  auto* editor = new QKeySequenceEdit(&host);
  auto* macroPage = new MacroRecordingPage(&host);
  macroPage->setSupported(true);
  layout->addWidget(standardButton);
  layout->addWidget(combo);
  layout->addWidget(spinBox);
  layout->addWidget(editor);
  layout->addWidget(macroPage);

  host.ensurePolished();
  for (auto* widget : host.findChildren<QWidget*>()) widget->ensurePolished();

  QCOMPARE(standardButton->sizeHint().height(), 40);
  QCOMPARE(combo->sizeHint().height(), 40);
  QCOMPARE(spinBox->sizeHint().height(), 40);
  QCOMPARE(editor->sizeHint().height(), 40);

  auto* recordButton =
      macroPage->findChild<QPushButton*>("macroRecordButton");
  auto* playButton =
      macroPage->findChild<QPushButton*>("macroPlayButton");
  QVERIFY(recordButton);
  QVERIFY(playButton);
  QCOMPARE(recordButton->sizeHint().height(), 44);
  QCOMPARE(playButton->sizeHint().height(), 44);
}
```

- [ ] **Step 3: Run the focused test and verify RED**

Run:

```bash
cmake --build build --target ClickFlowPageTests --parallel
```

Expected: FAIL at link time with an undefined `clickFlowStyleSheet()` symbol because the tested interface does not exist yet.

- [ ] **Step 4: Extract the application style into a testable unit**

Create `src/app/UiStyle.h`:

```cpp
#pragma once

#include <QString>

QString clickFlowStyleSheet();
```

Create `src/app/UiStyle.cpp`. Move the complete existing raw style string out of
`MainWindow::buildUi()` and return it from:

```cpp
#include "app/UiStyle.h"

QString clickFlowStyleSheet() {
  return QStringLiteral(R"(
    QMainWindow, #contentSurface { background: #f4f5f7; color: #18202b; }
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
  )");
}
```

Include `"app/UiStyle.h"` from `MainWindow.cpp` and replace the raw style call
with:

```cpp
setStyleSheet(clickFlowStyleSheet());
```

Add `src/app/UiStyle.cpp` to `APP_SOURCES`, add `src/app/UiStyle.h` to
`APP_HEADERS`, and add both files to the `ClickFlowPageTests` target so the
focused test links on macOS.

- [ ] **Step 5: Remove macro-page button sizing overrides**

Delete these two local rules from `MacroRecordingPage::MacroRecordingPage()`:

```css
#macroRecordButton { background: #2563eb; color: white; font-weight: 650;
  border: 0; border-radius: 8px; padding: 9px 18px; }
#macroPlayButton { background: #173b66; color: white; font-weight: 650;
  border: 0; border-radius: 8px; padding: 9px 18px; }
```

Keep the hotkey chip and error-label styles in the page-local style sheet.

- [ ] **Step 6: Run the focused test and verify GREEN**

Run:

```bash
cmake --build build --target ClickFlowPageTests --parallel
./build/ClickFlowPageTests controlsUseConsistentComfortableHeights
```

Expected: PASS.

- [ ] **Step 7: Run the full automated test suite**

Run:

```bash
ctest --preset test
```

Expected: all tests pass with zero failures.

- [ ] **Step 8: Perform macOS visual verification**

Launch `build/ClickFlow.app`, inspect all four pages, and capture screenshots. Verify:

- fixed-coordinate spin boxes and capture button align vertically;
- target-window and macro-library rows align vertically;
- preset action buttons share one height;
- hotkey editor heights match form controls;
- primary action text is not clipped;
- disabled controls remain legible;
- no horizontal scrollbar appears at the default 920×620 window size.

- [ ] **Step 9: Check the diff and commit**

Run:

```bash
git diff --check
git status --short
```

Then commit:

```bash
git add CMakeLists.txt tests/ClickFlowPageTests.cpp src/app/UiStyle.h \
  src/app/UiStyle.cpp src/app/MainWindow.cpp \
  src/app/pages/MacroRecordingPage.cpp
git commit -m "style: unify control sizing"
```
