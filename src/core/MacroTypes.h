#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QVector>

#include <optional>

enum class MacroEventType {
  KeyDown,
  KeyUp,
  MouseMove,
  MouseButtonDown,
  MouseButtonUp,
  Wheel,
  HorizontalWheel
};

enum class MacroMouseButton {
  None,
  Left,
  Right,
  Middle,
  X1,
  X2
};

enum class MacroTargetMode {
  Global,
  Window
};

struct WindowTarget {
  quintptr nativeId = 0;
  QString executablePath;
  QString className;
  QString title;
  QSize clientSize;
};

struct MacroEvent {
  MacroEventType type = MacroEventType::MouseMove;
  qint64 offsetUs = 0;
  quint32 virtualKey = 0;
  quint32 scanCode = 0;
  quint32 nativeFlags = 0;
  QPoint point;
  MacroMouseButton button = MacroMouseButton::None;
  int wheelDelta = 0;
};

struct MacroPlaybackSettings {
  double speed = 1.0;
  bool infinite = false;
  int repeatCount = 1;
  int loopDelayMs = 0;
};

struct MacroSequence {
  QString id;
  QString name;
  QDateTime createdAt;
  QDateTime modifiedAt;
  MacroTargetMode targetMode = MacroTargetMode::Global;
  WindowTarget target;
  qint64 durationUs = 0;
  QVector<MacroEvent> events;
  MacroPlaybackSettings playback;
};

Q_DECLARE_METATYPE(MacroEvent)
Q_DECLARE_METATYPE(MacroPlaybackSettings)
Q_DECLARE_METATYPE(WindowTarget)
Q_DECLARE_METATYPE(MacroSequence)

namespace MacroTypes {

QJsonObject toJson(const MacroSequence& sequence);
std::optional<MacroSequence> fromJson(const QJsonObject& object,
                                      QString* error = nullptr);
bool validate(const MacroSequence& sequence, QString* error = nullptr);

}  // namespace MacroTypes
