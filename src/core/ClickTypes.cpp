#include "core/ClickTypes.h"

#include <QStringList>

namespace ClickTypes {

QString buttonToString(ClickButton button) {
  return button == ClickButton::Left ? "left" : "right";
}

ClickButton buttonFromString(const QString& value) {
  return value.compare("right", Qt::CaseInsensitive) == 0 ? ClickButton::Right
                                                           : ClickButton::Left;
}

QString targetModeToString(TargetMode mode) {
  return mode == TargetMode::FixedPoint ? "fixed" : "follow";
}

TargetMode targetModeFromString(const QString& value) {
  return value.compare("fixed", Qt::CaseInsensitive) == 0 ? TargetMode::FixedPoint
                                                          : TargetMode::FollowCursor;
}

QString repeatModeToString(RepeatMode mode) {
  return mode == RepeatMode::Finite ? "finite" : "infinite";
}

RepeatMode repeatModeFromString(const QString& value) {
  return value.compare("finite", Qt::CaseInsensitive) == 0 ? RepeatMode::Finite
                                                            : RepeatMode::Infinite;
}

QVariantMap toVariantMap(const ClickProfile& profile) {
  QVariantMap data;
  data.insert("name", profile.name);
  data.insert("intervalMs", profile.intervalMs);
  data.insert("button", buttonToString(profile.button));
  data.insert("targetMode", targetModeToString(profile.targetMode));
  data.insert("fixedX", profile.fixedPoint.x());
  data.insert("fixedY", profile.fixedPoint.y());
  data.insert("repeatMode", repeatModeToString(profile.repeatMode));
  data.insert("repeatCount", profile.repeatCount);
  data.insert("jitterRadius", profile.jitterRadius);
  data.insert("countdownSeconds", profile.countdownSeconds);
  data.insert("alwaysOnTop", profile.alwaysOnTop);
  data.insert("hotkeyStartStop", profile.hotkeys.startStop);
  data.insert("hotkeyCapturePoint", profile.hotkeys.capturePoint);
  data.insert("hotkeyEmergencyStop", profile.hotkeys.emergencyStop);
  return data;
}

ClickProfile fromVariantMap(const QVariantMap& data) {
  ClickProfile profile;
  profile.name = data.value("name", profile.name).toString();
  profile.intervalMs = data.value("intervalMs", profile.intervalMs).toInt();
  profile.button = buttonFromString(data.value("button").toString());
  profile.targetMode = targetModeFromString(data.value("targetMode").toString());
  profile.fixedPoint = QPoint(
      data.value("fixedX", profile.fixedPoint.x()).toInt(),
      data.value("fixedY", profile.fixedPoint.y()).toInt());
  profile.repeatMode = repeatModeFromString(data.value("repeatMode").toString());
  profile.repeatCount = data.value("repeatCount", profile.repeatCount).toInt();
  profile.jitterRadius = data.value("jitterRadius", profile.jitterRadius).toInt();
  profile.countdownSeconds =
      data.value("countdownSeconds", profile.countdownSeconds).toInt();
  profile.alwaysOnTop = data.value("alwaysOnTop", profile.alwaysOnTop).toBool();
  profile.hotkeys.startStop =
      data.value("hotkeyStartStop", profile.hotkeys.startStop).toString();
  profile.hotkeys.capturePoint =
      data.value("hotkeyCapturePoint", profile.hotkeys.capturePoint).toString();
  profile.hotkeys.emergencyStop =
      data.value("hotkeyEmergencyStop", profile.hotkeys.emergencyStop).toString();
  return profile;
}

QString toDisplaySummary(const ClickProfile& profile) {
  QStringList parts;
  parts << QString("%1 毫秒").arg(profile.intervalMs)
        << (profile.button == ClickButton::Left ? "左键" : "右键")
        << (profile.targetMode == TargetMode::FollowCursor
                ? "跟随鼠标"
                : QString("固定坐标（%1, %2）")
                      .arg(profile.fixedPoint.x())
                      .arg(profile.fixedPoint.y()))
        << (profile.repeatMode == RepeatMode::Infinite
                ? "无限点击"
                : QString("%1 次").arg(profile.repeatCount));

  if (profile.jitterRadius > 0) {
    parts << QString("抖动 %1 像素").arg(profile.jitterRadius);
  }
  if (profile.countdownSeconds > 0) {
    parts << QString("倒计时 %1 秒").arg(profile.countdownSeconds);
  }

  return parts.join(" | ");
}

}  // namespace ClickTypes
