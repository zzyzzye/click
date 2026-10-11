#include "platform/windows/WindowsClickBackend.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <QKeySequence>

#include <QRandomGenerator>

#include <algorithm>
#include <optional>
#include <utility>

int defaultWindowsRandomOffset(int minimum, int maximumInclusive) {
  return QRandomGenerator::global()->bounded(minimum, maximumInclusive + 1);
}

WindowsClickBackend::WindowsClickBackend(std::unique_ptr<WindowsInputApi> api,
                                         RandomOffset randomOffset)
    : api_(std::move(api)), randomOffset_(std::move(randomOffset)) {}

bool WindowsClickBackend::click(const ClickProfile& profile) {
  if (!api_) {
    return false;
  }

  std::optional<QPoint> point;
  if (profile.targetMode == TargetMode::FollowCursor) {
    point = api_->cursorPosition();
  } else {
    point = profile.fixedPoint;
  }
  if (!point.has_value()) {
    return false;
  }

  const int radius = std::max(0, profile.jitterRadius);
  if (radius > 0) {
    point->rx() += randomOffset_(-radius, radius);
    point->ry() += randomOffset_(-radius, radius);
  }

  return api_->clickAt(*point, profile.button);
}

bool WindowsClickBackend::keyTap(const ClickProfile& profile) {
  return api_ && api_->keyTap(profile.keyboardKey);
}

QVector<QRect> WindowsClickBackend::screenRects() const {
  QVector<QRect> result;
  EnumDisplayMonitors(
      nullptr, nullptr,
      [](HMONITOR, HDC, LPRECT rect, LPARAM data) -> BOOL {
        auto* list = reinterpret_cast<QVector<QRect>*>(data);
        list->append(
            QRect(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top));
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&result));
  return result;
}
bool WindowsClickBackend::supportsKey(const QString& text) const {
  const auto sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
  if (sequence.count() != 1)
    return false;
  const int k = sequence[0].key();
  return (k >= Qt::Key_A && k <= Qt::Key_Z) || (k >= Qt::Key_0 && k <= Qt::Key_9) ||
         (k >= Qt::Key_F1 && k <= Qt::Key_F24) || k == Qt::Key_Space || k == Qt::Key_Return ||
         k == Qt::Key_Enter || k == Qt::Key_Tab || k == Qt::Key_Escape;
}
bool WindowsClickBackend::textInput(const QString& text) {
  return api_ && api_->textInput(text);
}

QPoint WindowsClickBackend::currentCursorPosition() const {
  if (!api_) {
    return QPoint();
  }
  return api_->cursorPosition().value_or(QPoint());
}

bool WindowsClickBackend::hasAccessibilityPermission() const {
  return true;
}

void WindowsClickBackend::requestAccessibilityPermission() {}
