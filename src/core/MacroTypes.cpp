#include "core/MacroTypes.h"

#include <QJsonArray>

#include <algorithm>
#include <array>

namespace {

constexpr int kSchemaVersion = 1;

QString eventTypeName(MacroEventType type) {
  switch (type) {
    case MacroEventType::KeyDown: return "keyDown";
    case MacroEventType::KeyUp: return "keyUp";
    case MacroEventType::MouseMove: return "mouseMove";
    case MacroEventType::MouseButtonDown: return "mouseButtonDown";
    case MacroEventType::MouseButtonUp: return "mouseButtonUp";
    case MacroEventType::Wheel: return "wheel";
    case MacroEventType::HorizontalWheel: return "horizontalWheel";
  }
  return {};
}

std::optional<MacroEventType> eventTypeFromName(const QString& name) {
  if (name == "keyDown") return MacroEventType::KeyDown;
  if (name == "keyUp") return MacroEventType::KeyUp;
  if (name == "mouseMove") return MacroEventType::MouseMove;
  if (name == "mouseButtonDown") return MacroEventType::MouseButtonDown;
  if (name == "mouseButtonUp") return MacroEventType::MouseButtonUp;
  if (name == "wheel") return MacroEventType::Wheel;
  if (name == "horizontalWheel") return MacroEventType::HorizontalWheel;
  return std::nullopt;
}

QString mouseButtonName(MacroMouseButton button) {
  switch (button) {
    case MacroMouseButton::None: return "none";
    case MacroMouseButton::Left: return "left";
    case MacroMouseButton::Right: return "right";
    case MacroMouseButton::Middle: return "middle";
    case MacroMouseButton::X1: return "x1";
    case MacroMouseButton::X2: return "x2";
  }
  return {};
}

std::optional<MacroMouseButton> mouseButtonFromName(const QString& name) {
  if (name == "none") return MacroMouseButton::None;
  if (name == "left") return MacroMouseButton::Left;
  if (name == "right") return MacroMouseButton::Right;
  if (name == "middle") return MacroMouseButton::Middle;
  if (name == "x1") return MacroMouseButton::X1;
  if (name == "x2") return MacroMouseButton::X2;
  return std::nullopt;
}

QString targetModeName(MacroTargetMode mode) {
  return mode == MacroTargetMode::Window ? "window" : "global";
}

std::optional<MacroTargetMode> targetModeFromName(const QString& name) {
  if (name == "global") return MacroTargetMode::Global;
  if (name == "window") return MacroTargetMode::Window;
  return std::nullopt;
}

void setError(QString* error, const QString& message) {
  if (error) *error = message;
}

bool supportedSpeed(double speed) {
  constexpr std::array<double, 4> speeds{0.5, 1.0, 1.5, 2.0};
  return std::any_of(speeds.cbegin(), speeds.cend(),
                     [speed](double value) { return qFuzzyCompare(speed, value); });
}

}  // namespace

QJsonObject MacroTypes::toJson(const MacroSequence& sequence) {
  QJsonArray events;
  for (const auto& event : sequence.events) {
    events.append(QJsonObject{
        {"type", eventTypeName(event.type)},
        {"offsetUs", static_cast<double>(event.offsetUs)},
        {"virtualKey", static_cast<double>(event.virtualKey)},
        {"scanCode", static_cast<double>(event.scanCode)},
        {"nativeFlags", static_cast<double>(event.nativeFlags)},
        {"x", event.point.x()},
        {"y", event.point.y()},
        {"button", mouseButtonName(event.button)},
        {"wheelDelta", event.wheelDelta},
    });
  }

  return QJsonObject{
      {"schemaVersion", kSchemaVersion},
      {"id", sequence.id},
      {"name", sequence.name},
      {"createdAt", sequence.createdAt.toUTC().toString(Qt::ISODateWithMs)},
      {"modifiedAt", sequence.modifiedAt.toUTC().toString(Qt::ISODateWithMs)},
      {"targetMode", targetModeName(sequence.targetMode)},
      {"target", QJsonObject{
                     {"executablePath", sequence.target.executablePath},
                     {"className", sequence.target.className},
                     {"title", sequence.target.title},
                     {"clientWidth", sequence.target.clientSize.width()},
                     {"clientHeight", sequence.target.clientSize.height()},
                 }},
      {"durationUs", static_cast<double>(sequence.durationUs)},
      {"playback", QJsonObject{
                       {"speed", sequence.playback.speed},
                       {"infinite", sequence.playback.infinite},
                       {"repeatCount", sequence.playback.repeatCount},
                       {"loopDelayMs", sequence.playback.loopDelayMs},
                   }},
      {"events", events},
  };
}

std::optional<MacroSequence> MacroTypes::fromJson(const QJsonObject& object,
                                                  QString* error) {
  if (object.value("schemaVersion").toInt(-1) != kSchemaVersion) {
    setError(error, "不支持的宏文件版本。");
    return std::nullopt;
  }

  MacroSequence sequence;
  sequence.id = object.value("id").toString();
  sequence.name = object.value("name").toString();
  sequence.createdAt = QDateTime::fromString(object.value("createdAt").toString(),
                                             Qt::ISODateWithMs);
  sequence.modifiedAt = QDateTime::fromString(object.value("modifiedAt").toString(),
                                              Qt::ISODateWithMs);
  const auto targetMode = targetModeFromName(object.value("targetMode").toString());
  if (!targetMode) {
    setError(error, "宏文件包含无效的目标模式。");
    return std::nullopt;
  }
  sequence.targetMode = *targetMode;

  const QJsonObject target = object.value("target").toObject();
  sequence.target.executablePath = target.value("executablePath").toString();
  sequence.target.className = target.value("className").toString();
  sequence.target.title = target.value("title").toString();
  sequence.target.clientSize = QSize(target.value("clientWidth").toInt(),
                                     target.value("clientHeight").toInt());
  sequence.durationUs = static_cast<qint64>(object.value("durationUs").toDouble(-1));

  const QJsonObject playback = object.value("playback").toObject();
  sequence.playback.speed = playback.value("speed").toDouble(0.0);
  sequence.playback.infinite = playback.value("infinite").toBool(false);
  sequence.playback.repeatCount = playback.value("repeatCount").toInt(0);
  sequence.playback.loopDelayMs = playback.value("loopDelayMs").toInt(-1);

  for (const auto& value : object.value("events").toArray()) {
    const QJsonObject eventObject = value.toObject();
    const auto type = eventTypeFromName(eventObject.value("type").toString());
    const auto button = mouseButtonFromName(eventObject.value("button").toString());
    if (!type || !button) {
      setError(error, "宏文件包含无效的事件类型。");
      return std::nullopt;
    }
    MacroEvent event;
    event.type = *type;
    event.offsetUs = static_cast<qint64>(eventObject.value("offsetUs").toDouble(-1));
    event.virtualKey = static_cast<quint32>(eventObject.value("virtualKey").toDouble());
    event.scanCode = static_cast<quint32>(eventObject.value("scanCode").toDouble());
    event.nativeFlags = static_cast<quint32>(eventObject.value("nativeFlags").toDouble());
    event.point = QPoint(eventObject.value("x").toInt(), eventObject.value("y").toInt());
    event.button = *button;
    event.wheelDelta = eventObject.value("wheelDelta").toInt();
    sequence.events.append(event);
  }

  if (!validate(sequence, error)) return std::nullopt;
  return sequence;
}

bool MacroTypes::validate(const MacroSequence& sequence, QString* error) {
  const QString id = sequence.id.trimmed();
  if (id.isEmpty() || id.contains('/') || id.contains('\\')) {
    setError(error, "宏标识无效。");
    return false;
  }
  if (sequence.name.trimmed().isEmpty()) {
    setError(error, "宏名称不能为空。");
    return false;
  }
  if (!sequence.createdAt.isValid() || !sequence.modifiedAt.isValid()) {
    setError(error, "宏时间信息无效。");
    return false;
  }
  if (sequence.targetMode == MacroTargetMode::Window &&
      (sequence.target.executablePath.isEmpty() || sequence.target.className.isEmpty() ||
       sequence.target.title.isEmpty() || !sequence.target.clientSize.isValid() ||
       sequence.target.clientSize.isEmpty())) {
    setError(error, "绑定窗口信息不完整。");
    return false;
  }
  if (!supportedSpeed(sequence.playback.speed) || sequence.playback.repeatCount < 1 ||
      sequence.playback.loopDelayMs < 0) {
    setError(error, "宏回放设置无效。");
    return false;
  }
  if (sequence.events.isEmpty() || sequence.durationUs < 0) {
    setError(error, "宏没有可回放的事件。");
    return false;
  }

  qint64 previousOffset = -1;
  for (const auto& event : sequence.events) {
    if (event.offsetUs < 0 || event.offsetUs < previousOffset) {
      setError(error, "宏事件时间顺序无效。");
      return false;
    }
    previousOffset = event.offsetUs;
  }
  if (sequence.durationUs < previousOffset) {
    setError(error, "宏总时长小于最后一个事件时间。");
    return false;
  }
  return true;
}
