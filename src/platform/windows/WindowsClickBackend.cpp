#include "platform/windows/WindowsClickBackend.h"

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
