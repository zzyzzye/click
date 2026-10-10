#include "app/widgets/SmoothScrollArea.h"

#include <algorithm>

#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

namespace {

constexpr int kAnimationDurationMs = 160;
constexpr int kPixelsPerWheelStep = 37;

int verticalDelta(const QWheelEvent* event) {
  if (!event->pixelDelta().isNull()) return event->pixelDelta().y();
  return event->angleDelta().y() * kPixelsPerWheelStep / 120;
}

}  // namespace

SmoothScrollArea::SmoothScrollArea(QWidget* parent) : QScrollArea(parent) {
  scrollAnimation_ = new QPropertyAnimation(verticalScrollBar(), "value", this);
  scrollAnimation_->setDuration(kAnimationDurationMs);
  scrollAnimation_->setEasingCurve(QEasingCurve::OutCubic);
}

void SmoothScrollArea::wheelEvent(QWheelEvent* event) {
  if (!scrollForWheelEvent(*event)) {
    QScrollArea::wheelEvent(event);
  }
}

void SmoothScrollArea::setReducedMotion(bool reduced) {
  reducedMotion_ = reduced;
  if (reduced) scrollAnimation_->stop();
}

bool SmoothScrollArea::scrollForWheelEvent(const QWheelEvent& event) {
  const int delta = verticalDelta(&event);
  if (delta == 0 || std::abs(event.angleDelta().x()) > std::abs(event.angleDelta().y())) {
    return false;
  }
  QScrollBar* const bar = verticalScrollBar();
  const int start = bar->value();
  // 触控板已提供连续像素位移，不再叠加动画延迟。
  if (reducedMotion_ || !event.pixelDelta().isNull()) {
    scrollAnimation_->stop();
    bar->setValue(std::clamp(start - delta, bar->minimum(), bar->maximum()));
    return true;
  }
  const bool animating = scrollAnimation_->state() == QAbstractAnimation::Running;
  const int previousTarget = animating ? scrollAnimation_->endValue().toInt() : start;
  // 同方向快速滚动累积位移；反向时从当前位置立即响应。
  const bool sameDirection = (previousTarget - start) * -delta > 0;
  const int base = sameDirection ? previousTarget : start;
  const int target = std::clamp(base - delta, bar->minimum(), bar->maximum());
  scrollAnimation_->stop();
  if (target != start) {
    scrollAnimation_->setStartValue(start);
    scrollAnimation_->setEndValue(target);
    scrollAnimation_->start();
  }
  return true;
}
