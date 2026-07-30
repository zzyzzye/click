#pragma once

#include <QPoint>
#include <QString>
#include <QVariantMap>

enum class ClickButton {
  Left,
  Right
};

enum class InputMode {
  Mouse,
  Keyboard
};

enum class TargetMode {
  FollowCursor,
  FixedPoint
};

enum class RepeatMode {
  Infinite,
  Finite
};

struct HotkeyBindings {
  QString startStop = "F6";
  QString capturePoint = "F7";
  QString emergencyStop = "F8";
};

struct ClickProfile {
  QString name = "Default";
  int intervalMs = 100;
  InputMode inputMode = InputMode::Mouse;
  QString keyboardKey = "Space";
  ClickButton button = ClickButton::Left;
  TargetMode targetMode = TargetMode::FollowCursor;
  QPoint fixedPoint = QPoint(0, 0);
  RepeatMode repeatMode = RepeatMode::Infinite;
  int repeatCount = 10;
  int jitterRadius = 0;
  int countdownSeconds = 0;
  bool alwaysOnTop = false;
  HotkeyBindings hotkeys;
};

namespace ClickTypes {

QString buttonToString(ClickButton button);
ClickButton buttonFromString(const QString& value);

QString targetModeToString(TargetMode mode);
TargetMode targetModeFromString(const QString& value);

QString repeatModeToString(RepeatMode mode);
RepeatMode repeatModeFromString(const QString& value);

QVariantMap toVariantMap(const ClickProfile& profile);
ClickProfile fromVariantMap(const QVariantMap& data);
QString toDisplaySummary(const ClickProfile& profile);

}  // namespace ClickTypes

