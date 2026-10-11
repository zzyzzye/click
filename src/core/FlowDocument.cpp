#include "core/FlowDocument.h"

#include <QJsonArray>
#include <QSet>
#include <QUuid>
#include <cmath>
#include <limits>

namespace {
QString uuid() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
bool fail(QString* error, const QString& message) {
  if (error)
    *error = message;
  return false;
}
bool safeId(const QString& id) {
  return !id.isEmpty() && id != "." && id != ".." && !id.contains('/') && !id.contains('\\');
}
bool integer(const QJsonValue& value, double minimum, double maximum) {
  if (!value.isDouble())
    return false;
  const double n = value.toDouble();
  return std::isfinite(n) && n == std::floor(n) && n >= minimum && n <= maximum;
}
} // namespace

FlowDocument FlowTypes::create() {
  FlowDocument d;
  d.id = uuid();
  return d;
}
FlowStep FlowTypes::createStep(FlowStepKind kind) {
  FlowStep s;
  s.id = uuid();
  s.kind = kind;
  return s;
}

bool FlowTypes::validate(const FlowDocument& d, QString* error, int* invalidStep) {
  if (invalidStep)
    *invalidStep = -1;
  if (!safeId(d.id) || d.name.trimmed().isEmpty())
    return fail(error, "请填写流程名称。");
  if (d.steps.isEmpty() || d.steps.size() > 1000)
    return fail(error, "请添加 1～1000 个步骤。");
  if (d.playback.repeatCount < 1 || d.playback.repeatCount > 1000000 ||
      d.playback.loopDelayMs < 0 || d.playback.loopDelayMs > 3600000 || d.playback.speed != 1.0)
    return fail(error, "流程循环设置无效。");
  QSet<QString> ids;
  qint64 events = 0;
  long double durationUs = 0;
  for (int i = 0; i < d.steps.size(); ++i) {
    const auto& s = d.steps[i];
    QString reason;
    if (!safeId(s.id) || ids.contains(s.id))
      reason = "步骤标识重复或无效。";
    ids.insert(s.id);
    if (int(s.kind) < 0 || int(s.kind) > int(FlowStepKind::Recording))
      reason = "未知步骤类型。";
    if (s.waitMs < 0 || s.waitMs > 3600000)
      reason = "等待时间应在 0～3600000 毫秒之间。";
    if (s.kind == FlowStepKind::Click &&
        (s.count < 1 || s.count > 10000 ||
         (s.button != MacroMouseButton::Left && s.button != MacroMouseButton::Right)))
      reason = "点击次数应在 1～10000 之间，按键请选择左键或右键。";
    if (s.kind == FlowStepKind::Key && s.value.trimmed().isEmpty())
      reason = "请选择按键或组合键。";
    if (s.kind == FlowStepKind::Text && (s.value.isEmpty() || s.value.size() > 10000))
      reason = "请输入 1～10000 个字符。";
    if (s.kind == FlowStepKind::Recording) {
      MacroTypes::validate(s.recording, &reason);
      for (const auto& e : s.recording.events) {
        if (int(e.type) < 0 || int(e.type) > int(MacroEventType::HorizontalWheel)) {
          reason = "原始录制包含未知事件，无法安全执行。";
          break;
        }
      }
    }
    events += s.kind == FlowStepKind::Recording ? s.recording.events.size() + 2
              : s.kind == FlowStepKind::Click   ? s.count + 2
              : s.kind == FlowStepKind::Text    ? s.value.size() + 2
                                                : 3;
    durationUs +=
        s.kind == FlowStepKind::Recording
            ? static_cast<long double>(s.recording.durationUs) / s.recording.playback.speed
        : s.kind == FlowStepKind::Click
            ? static_cast<long double>(s.count) * 10000 + qint64(s.waitMs) * 1000
        : s.kind == FlowStepKind::Wait ? qint64(s.waitMs) * 1000
        : s.kind == FlowStepKind::Text ? s.value.size() * 1000
                                       : 10000;
    if (!reason.isEmpty()) {
      if (invalidStep)
        *invalidStep = i;
      return fail(error, QString("步骤 %1：%2").arg(i + 1).arg(reason));
    }
  }
  if (events > 200000)
    return fail(error, "流程过大，请减少点击次数、文字或录制事件。");
  if (!std::isfinite(durationUs) || durationUs > std::numeric_limits<qint64>::max() / 4.0L)
    return fail(error, "流程单轮总时长过大，请缩短等待或录制块。");
  return true;
}

QString FlowTypes::summary(const FlowStep& s) {
  switch (s.kind) {
  case FlowStepKind::Click:
    return QString("点击 (%1, %2) · %3 · %4 次 · 等待 %5 毫秒")
        .arg(s.point.x())
        .arg(s.point.y())
        .arg(s.button == MacroMouseButton::Left ? "左键" : "右键")
        .arg(s.count)
        .arg(s.waitMs);
  case FlowStepKind::Key:
    return QString("按键 %1").arg(s.value);
  case FlowStepKind::Text:
    return QString("输入文字 · %1 个字符").arg(s.value.size());
  case FlowStepKind::Wait:
    return QString("等待 %1 毫秒").arg(s.waitMs);
  case FlowStepKind::Recording:
    return QString("原始录制 · %1 个事件 · 完整保留").arg(s.recording.events.size());
  }
  return "未知步骤";
}

QJsonObject FlowTypes::toJson(const FlowDocument& d) {
  QJsonArray steps, screens;
  for (const auto& r : d.screens)
    screens.append(QJsonArray{r.x(), r.y(), r.width(), r.height()});
  for (const auto& s : d.steps) {
    QJsonObject o{{"id", s.id},         {"kind", int(s.kind)},     {"x", s.point.x()},
                  {"y", s.point.y()},   {"button", int(s.button)}, {"count", s.count},
                  {"waitMs", s.waitMs}, {"value", s.value}};
    if (s.kind == FlowStepKind::Recording)
      o.insert("recording", MacroTypes::toJson(s.recording));
    steps.append(o);
  }
  return {{"format", "ClickFlow.Flow"},
          {"version", 1},
          {"id", d.id},
          {"name", d.name},
          {"steps", steps},
          {"screens", screens},
          {"infinite", d.playback.infinite},
          {"rounds", d.playback.repeatCount},
          {"loopDelayMs", d.playback.loopDelayMs}};
}

std::optional<FlowDocument> FlowTypes::fromJson(const QJsonObject& o, QString* error) {
  if (o.value("format").toString() != "ClickFlow.Flow" || !integer(o.value("version"), 1, 1)) {
    fail(error, "不支持的流程文件格式或版本，原文件未修改。");
    return {};
  }
  if (!o.value("steps").isArray() || !o.value("screens").isArray() ||
      !o.value("infinite").isBool() || !integer(o.value("rounds"), 1, 1000000) ||
      !integer(o.value("loopDelayMs"), 0, 3600000)) {
    fail(error, "流程步骤、布局或循环参数格式损坏。");
    return {};
  }
  FlowDocument d;
  d.id = o.value("id").toString();
  d.name = o.value("name").toString();
  d.playback.infinite = o.value("infinite").toBool();
  d.playback.repeatCount = o.value("rounds").toInt(-1);
  d.playback.loopDelayMs = o.value("loopDelayMs").toInt(-1);
  for (const auto& value : o.value("screens").toArray()) {
    const auto r = value.toArray();
    if (r.size() != 4) {
      fail(error, "显示器布局数据损坏。");
      return {};
    }
    if (!integer(r[0], -100000, 100000) || !integer(r[1], -100000, 100000) ||
        !integer(r[2], 1, 200000) || !integer(r[3], 1, 200000)) {
      fail(error, "显示器坐标和尺寸必须为有效整数。");
      return {};
    }
    QRect rect(r[0].toInt(), r[1].toInt(), r[2].toInt(), r[3].toInt());
    if (!rect.isValid()) {
      fail(error, "显示器布局数据无效。");
      return {};
    }
    d.screens.append(rect);
  }
  for (const auto& value : o.value("steps").toArray()) {
    const auto item = value.toObject();
    if (!integer(item.value("kind"), 0, 4) || !integer(item.value("x"), -100000, 100000) ||
        !integer(item.value("y"), -100000, 100000) || !integer(item.value("count"), 1, 10000) ||
        !integer(item.value("waitMs"), 0, 3600000) || !item.value("value").isString()) {
      fail(error, QString("步骤 %1：类型或参数格式无效，请检查流程文件。").arg(d.steps.size() + 1));
      return {};
    }
    FlowStep s;
    s.id = item.value("id").toString();
    s.kind = FlowStepKind(item.value("kind").toInt(-1));
    s.point = {item.value("x").toInt(), item.value("y").toInt()};
    s.button = MacroMouseButton(item.value("button").toInt(-1));
    s.count = item.value("count").toInt(-1);
    s.waitMs = item.value("waitMs").toInt(-1);
    s.value = item.value("value").toString();
    if (s.kind == FlowStepKind::Recording) {
      auto raw = MacroTypes::fromJson(item.value("recording").toObject(), error);
      if (!raw)
        return {};
      s.recording = *raw;
    }
    d.steps.append(s);
  }
  if (!validate(d, error))
    return {};
  return d;
}

std::optional<MacroSequence> FlowTypes::compile(const FlowDocument& d, QString* error) {
  if (!validate(d, error))
    return {};
  MacroSequence result;
  result.id = d.id;
  result.name = d.name;
  result.createdAt = result.modifiedAt = QDateTime::currentDateTimeUtc();
  result.playback = d.playback;
  qint64 time = 0;
  for (int i = 0; i < d.steps.size(); ++i) {
    const auto& s = d.steps[i];
    auto append = [&](MacroEventType type, bool safe, const QString& text = {}, qint64 delay = 0,
                      bool relative = true) {
      MacroEvent e;
      e.type = type;
      e.offsetUs = time;
      e.stepIndex = i;
      e.point = s.point;
      e.button = s.button;
      e.text = text;
      e.pauseBoundary = safe;
      e.relativeDelay = relative;
      e.delayUs = delay;
      result.events.append(e);
    };
    append(MacroEventType::Boundary, true);
    result.events.last().timelineAnchor = true;
    qint64 endDelay = 0;
    switch (s.kind) {
    case FlowStepKind::Click:
      for (int n = 0; n < s.count; ++n) {
        append(MacroEventType::ActionClick, true, {}, n ? 10000 : 0);
        time += 10000;
      }
      endDelay = 10000 + qint64(s.waitMs) * 1000;
      time += qint64(s.waitMs) * 1000;
      break;
    case FlowStepKind::Key:
      append(MacroEventType::ActionKey, true, s.value);
      time += 10000;
      endDelay = 10000;
      break;
    case FlowStepKind::Text:
      // 每次只注入一个 Unicode 字符，长文本也能及时暂停和停止。
      for (int n = 0; n < s.value.size(); ++n) {
        const bool first = n == 0;
        QString character(s.value[n]);
        if (s.value[n].isHighSurrogate() && n + 1 < s.value.size() &&
            s.value[n + 1].isLowSurrogate())
          character += s.value[++n];
        append(MacroEventType::ActionText, true, character, first ? 0 : 1000);
        time += 1000;
      }
      endDelay = 1000;
      break;
    case FlowStepKind::Wait:
      endDelay = qint64(s.waitMs) * 1000;
      time += endDelay;
      break;
    case FlowStepKind::Recording:
      for (auto e : s.recording.events) {
        e.offsetUs = time + qint64(std::llround(e.offsetUs / s.recording.playback.speed));
        e.stepIndex = i;
        e.pauseBoundary = false;
        result.events.append(e);
      }
      time += qint64(std::llround(s.recording.durationUs / s.recording.playback.speed));
      break;
    }
    append(MacroEventType::Boundary, true, {}, endDelay, s.kind != FlowStepKind::Recording);
  }
  result.durationUs = time;
  return result;
}
