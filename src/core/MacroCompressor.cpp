#include "core/MacroCompressor.h"

#include <QSet>

#include <algorithm>
#include <cmath>
#include <optional>

namespace {

bool isMove(const MacroEvent& event) {
  return event.type == MacroEventType::MouseMove;
}

bool isKeyboard(const MacroEvent& event) {
  return event.type == MacroEventType::KeyDown || event.type == MacroEventType::KeyUp;
}

bool nearlyCollinear(const QPoint& first, const QPoint& middle,
                     const QPoint& last) {
  const qint64 dx = last.x() - first.x();
  const qint64 dy = last.y() - first.y();
  const qint64 cross = std::abs(dx * (middle.y() - first.y()) -
                                dy * (middle.x() - first.x()));
  const double length = std::sqrt(static_cast<double>(dx * dx + dy * dy));
  if (length == 0.0) return true;
  const double distance = static_cast<double>(cross) / length;
  const bool between =
      middle.x() >= std::min(first.x(), last.x()) &&
      middle.x() <= std::max(first.x(), last.x()) &&
      middle.y() >= std::min(first.y(), last.y()) &&
      middle.y() <= std::max(first.y(), last.y());
  return between && distance <= 1.0;
}

std::optional<quint32> virtualKeyForText(const QString& text) {
  const QString key = text.trimmed();
  if (key.size() == 1 && key[0].isLetterOrNumber()) {
    return static_cast<quint32>(key[0].toUpper().unicode());
  }
  if (key.startsWith('F', Qt::CaseInsensitive)) {
    bool ok = false;
    const int number = key.mid(1).toInt(&ok);
    if (ok && number >= 1 && number <= 24) {
      return static_cast<quint32>(0x70 + number - 1);
    }
  }
  if (key.compare("Space", Qt::CaseInsensitive) == 0) return 0x20;
  if (key.compare("Return", Qt::CaseInsensitive) == 0 ||
      key.compare("Enter", Qt::CaseInsensitive) == 0) return 0x0d;
  if (key.compare("Esc", Qt::CaseInsensitive) == 0 ||
      key.compare("Escape", Qt::CaseInsensitive) == 0) return 0x1b;
  return std::nullopt;
}

struct ReservedChord {
  quint32 mainKey = 0;
  QSet<quint32> allowedKeys;
  QList<QSet<quint32>> requiredModifierGroups;
};

std::optional<ReservedChord> parseChord(const QString& text) {
  if (text.contains(',')) return std::nullopt;
  const QStringList parts = text.split('+', Qt::SkipEmptyParts);
  if (parts.isEmpty()) return std::nullopt;
  const auto mainKey = virtualKeyForText(parts.last());
  if (!mainKey) return std::nullopt;

  ReservedChord chord;
  chord.mainKey = *mainKey;
  chord.allowedKeys.insert(*mainKey);
  const auto addModifiers = [&chord](QSet<quint32> values) {
    chord.allowedKeys.unite(values);
    chord.requiredModifierGroups.append(std::move(values));
  };
  for (qsizetype index = 0; index + 1 < parts.size(); ++index) {
    const QString modifier = parts[index].trimmed();
    if (modifier.compare("Shift", Qt::CaseInsensitive) == 0) {
      addModifiers({0x10, 0xa0, 0xa1});
    } else if (modifier.compare("Ctrl", Qt::CaseInsensitive) == 0 ||
               modifier.compare("Control", Qt::CaseInsensitive) == 0) {
      addModifiers({0x11, 0xa2, 0xa3});
    } else if (modifier.compare("Alt", Qt::CaseInsensitive) == 0) {
      addModifiers({0x12, 0xa4, 0xa5});
    } else if (modifier.compare("Meta", Qt::CaseInsensitive) == 0 ||
               modifier.compare("Win", Qt::CaseInsensitive) == 0) {
      addModifiers({0x5b, 0x5c});
    } else {
      return std::nullopt;
    }
  }
  return chord;
}

}  // namespace

QVector<MacroEvent> MacroCompressor::compress(const QVector<MacroEvent>& events) {
  QVector<MacroEvent> sampled;
  sampled.reserve(events.size());
  int pressedButtons = 0;
  std::optional<MacroEvent> pendingMove;

  const auto flushPending = [&sampled, &pendingMove]() {
    if (!pendingMove) return;
    if (sampled.isEmpty() || !isMove(sampled.last()) ||
        sampled.last().point != pendingMove->point) {
      sampled.append(*pendingMove);
    }
    pendingMove.reset();
  };

  for (const auto& event : events) {
    if (!isMove(event)) {
      flushPending();
      sampled.append(event);
      if (event.type == MacroEventType::MouseButtonDown) ++pressedButtons;
      if (event.type == MacroEventType::MouseButtonUp) {
        pressedButtons = std::max(0, pressedButtons - 1);
      }
      continue;
    }

    if (sampled.isEmpty() || !isMove(sampled.last())) {
      sampled.append(event);
      continue;
    }
    const auto& previous = sampled.last();
    const qint64 intervalUs = pressedButtons > 0 ? 8000 : 16000;
    const int distanceThreshold = pressedButtons > 0 ? 2 : 8;
    const int dx = event.point.x() - previous.point.x();
    const int dy = event.point.y() - previous.point.y();
    if (event.offsetUs - previous.offsetUs >= intervalUs ||
        dx * dx + dy * dy >= distanceThreshold * distanceThreshold) {
      sampled.append(event);
      pendingMove.reset();
    } else {
      pendingMove = event;
    }
  }
  flushPending();

  QVector<MacroEvent> simplified;
  simplified.reserve(sampled.size());
  for (const auto& event : sampled) {
    if (isMove(event) && simplified.size() >= 2 &&
        isMove(simplified[simplified.size() - 1]) &&
        isMove(simplified[simplified.size() - 2]) &&
        nearlyCollinear(simplified[simplified.size() - 2].point,
                        simplified[simplified.size() - 1].point, event.point)) {
      simplified.last() = event;
    } else {
      simplified.append(event);
    }
  }
  return simplified;
}

QVector<MacroEvent> MacroCompressor::removeReservedTail(
    const QVector<MacroEvent>& events, const QStringList& reservedHotkeys) {
  for (const QString& hotkey : reservedHotkeys) {
    const auto chord = parseChord(hotkey);
    if (!chord) continue;
    bool mainDown = false;
    bool mainUp = false;
    QSet<quint32> seenKeys;
    qsizetype firstIndex = events.size();
    for (qsizetype index = events.size(); index > 0; --index) {
      const auto& event = events[index - 1];
      if (!isKeyboard(event) || !chord->allowedKeys.contains(event.virtualKey)) break;
      firstIndex = index - 1;
      seenKeys.insert(event.virtualKey);
      if (event.virtualKey == chord->mainKey) {
        mainDown |= event.type == MacroEventType::KeyDown;
        mainUp |= event.type == MacroEventType::KeyUp;
      }
    }
    const bool hasModifiers = std::all_of(
        chord->requiredModifierGroups.cbegin(), chord->requiredModifierGroups.cend(),
        [&seenKeys](const QSet<quint32>& group) {
          return std::any_of(group.cbegin(), group.cend(),
                             [&seenKeys](quint32 key) { return seenKeys.contains(key); });
        });
    if (firstIndex < events.size() && mainDown && mainUp && hasModifiers) {
      return events.mid(0, firstIndex);
    }
  }
  return events;
}
