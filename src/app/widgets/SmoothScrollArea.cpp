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

bool SmoothScrollArea::scrollForWheelEvent(const QWheelEvent& event) {
  const int delta = verticalDelta(&event);
  if (delta == 0 || std::abs(event.angleDelta().x()) > std::abs(event.angleDelta().y())) {
    return false;
  }
  QScrollBar* const bar = verticalScrollBar();
  const int start = bar->value();
  const int target = std::clamp(start - delta, bar->minimum(), bar->maximum());
  if (target != start) {
    scrollAnimation_->stop();
    scrollAnimation_->setStartValue(start);
    scrollAnimation_->setEndValue(target);
    scrollAnimation_->start();
  }
  return true;
}
