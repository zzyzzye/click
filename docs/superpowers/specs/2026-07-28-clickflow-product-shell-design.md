# ClickFlow Product Shell Design

## Goal

Replace the temporary “极简连点器” presentation with a coherent ClickFlow
product shell, an iPad Settings-inspired navigation model, and a single
project-wide version source. This iteration changes information architecture,
visual hierarchy, naming, and version propagation. It does not add new input
features or produce a new distribution package.

## Product Identity

The user-visible product name is **ClickFlow**. The main window title,
application display name, About content, and future packaged executable use
this name.

The existing `QtClicker` settings application key remains unchanged so users
keep profiles saved by earlier builds. Renaming the settings namespace is
outside this iteration because it would require an explicit migration path.
Internal CMake target names may remain `QtClicker` where changing them would
only add churn; the built executable can use `OUTPUT_NAME ClickFlow`.

Chinese interface text describes the product as “连点器” where a functional
description is useful, but “极简” is removed from product copy and profile
names.

## Version Strategy

The next application version is **0.2.0**. Versions follow Semantic Versioning:

- patch releases fix behavior without changing supported workflows;
- minor releases add backward-compatible capabilities or meaningful product
  improvements;
- `1.0.0` is reserved for a release that has completed real Windows and macOS
  usage validation and has a stable settings contract.

`project(ClickFlow VERSION 0.2.0 LANGUAGES CXX)` in the top-level CMake file is
the single version source. CMake generates a small version header consumed by
the application. Startup sets `QCoreApplication::applicationVersion()` from
that generated value. The About page reads the Qt application version instead
of duplicating a literal.

On Windows, a generated `.rc` resource embeds the same version in
`FileVersion`, `ProductVersion`, `ProductName`, and `FileDescription`.
Future release tags use the form `v0.2.0`.

## Window Structure

The default window is approximately 920 by 620 logical pixels, with a sensible
minimum size that keeps all settings usable. The shell has four persistent
regions:

1. a left navigation sidebar;
2. a compact status strip at the top of the content area;
3. a stacked page area;
4. a fixed action bar at the bottom.

The action bar and status strip remain visible while pages change. Starting or
stopping does not move the primary action or resize the window.

## Sidebar

The sidebar is approximately 180 logical pixels wide and uses a quiet neutral
surface distinct from the content background. Its header shows:

- ClickFlow;
- version `0.2.0`, sourced from the application version;
- a short “连点器” descriptor.

Navigation contains exactly three entries:

- 连点设置;
- 热键;
- 预设与关于.

Entries use text and restrained Qt-standard icons when an appropriate platform
icon exists. Navigation labels do not depend on icon recognition. The active
entry has one clear accent treatment rather than multiple borders.

## Status Strip

The top status strip summarizes:

- input-control availability;
- controller state;
- countdown or remaining-click information.

It uses short labels and status colors with sufficient text contrast. Windows
does not show an inactive permission button. macOS may expose the permission
action only when permission is missing.

The current oversized “怎么用” block is removed from the permanent shell.
Short first-use guidance belongs on the Click Settings page and should not
consume vertical space after the controls are understood.

## Click Settings Page

This is the default page. It contains two visually separated cards:

### Basic Click Card

- click interval;
- left or right mouse button;
- follow-cursor or fixed-position mode;
- fixed X and Y coordinates;
- capture-current-position action.

Fixed-position controls remain visible but disabled in follow-cursor mode. This
keeps layout stable and communicates that the capability exists.

### Run Behavior Card

- infinite or finite repeat mode;
- repeat count;
- jitter radius;
- countdown;
- always-on-top.

Rows use a consistent label column and a control column. Related controls may
share one row, such as X/Y coordinates, but unrelated settings do not.
Finite-count and fixed-position dependencies preserve the existing enable and
disable behavior.

## Hotkeys Page

The Hotkeys page contains one card with three rows:

- start/stop;
- capture position;
- emergency stop.

Each row includes a concise explanation beneath or beside the editor. A small
footer states that global hotkeys can fail when another application owns the
same sequence. Validation errors continue to identify the offending sequence.

## Presets and About Page

This page uses two cards rather than one large empty list:

### Presets Card

The list receives the available space within the upper card. New, Save, Rename,
Delete, and Load actions appear in one compact toolbar. Actions that require a
selection remain disabled until a preset is selected.

### About Card

The lower card shows:

- ClickFlow;
- version from `QCoreApplication::applicationVersion()`;
- current platform and Qt version;
- a short description of local settings storage.

It does not present packaging, update checking, telemetry, or release-download
controls in this iteration.

## Action Bar

The bottom action bar contains:

- the primary Start/Stop button;
- a concise current-mode summary, such as interval and target mode;
- emergency-stop guidance.

The Start/Stop button has the strongest visual emphasis in the application.
Configuration pages remain visible while running, but editable controls and
preset mutation actions are disabled. Navigation remains available so users
can inspect the active configuration.

## Component Boundaries

`MainWindow` becomes the composition and coordination layer rather than the
owner of every control. The UI is split into focused widgets:

- `NavigationSidebar`: product header, version, and page selection;
- `StatusStrip`: permission and controller status;
- `ClickSettingsPage`: click and run-behavior controls;
- `HotkeySettingsPage`: the three global hotkey editors;
- `PresetsAboutPage`: preset actions and version/platform information;
- `ActionBar`: mode summary and Start/Stop action.

Pages expose profile-oriented methods and signals rather than individual
widgets:

```cpp
void setProfile(const ClickProfile& profile);
void applyToProfile(ClickProfile& profile) const;
```

`MainWindow` continues to own `ClickController`, platform services, and
`SettingsRepository`. It maps page signals to the existing handlers and is the
only component that starts or stops the controller.

## Visual Language

The UI uses Qt stylesheets sparingly and keeps native control behavior:

- neutral gray sidebar and content background;
- white or platform-appropriate card surfaces;
- one accent color for selection and primary action;
- 8-pixel spacing rhythm with larger 16/24-pixel section gaps;
- modest corner radii and subtle borders;
- no nested `QGroupBox` frames;
- no decorative gradients, oversized hero areas, or dense icon grids.

The layout must remain readable at 100%, 125%, and 150% Windows display
scaling. Fixed pixel values are limited to intentional shell dimensions such as
sidebar width; text-bearing controls use layout size hints.

## Compatibility and State

- Existing profile serialization remains unchanged.
- Existing `QtClicker` QSettings keys remain unchanged.
- Existing macOS and Windows platform backends remain unchanged.
- Existing default hotkeys remain F6, F7, and F8.
- `.gitignore` continues to ignore `/dist/` and `CMakeUserPresets.json`.
- No packaging command is run as part of this iteration.
- Existing local `dist/` output is not committed or treated as a release.

## Testing

Automated tests cover:

- CMake version propagation into the running Qt application;
- ClickFlow window title and About version;
- the three sidebar entries and stacked-page switching;
- persistent visibility of the status strip and action bar;
- complete profile round-trip through the split pages;
- fixed-position and finite-repeat enablement;
- F7 capture behavior;
- preset selection action enablement;
- running-state edit locks while navigation stays enabled;
- existing controller, platform, hotkey, and settings tests.

Visual verification checks the Windows Debug application at 920 by 620 and at
125% display scaling when available. It confirms there is no clipped text,
overlap, unexpected scrollbar, empty oversized region, or modal dialog on
startup. macOS visual verification is reported separately and is not claimed
from a Windows-only machine.

## Acceptance Criteria

1. The application opens as ClickFlow 0.2.0 with no “极简” product copy.
2. Users navigate between exactly three sidebar pages.
3. Status and Start/Stop controls remain visible on every page.
4. All existing profile fields remain editable and persist with no schema
   change.
5. Presets no longer dominate the default page.
6. Version values shown by the app and Windows file metadata originate from the
   CMake project version.
7. Windows build and all automated tests pass.
8. No packaging step is performed for this UI/version iteration.
