#pragma once

#include <QScrollArea>

class QPropertyAnimation;
class QWheelEvent;

class SmoothScrollArea final : public QScrollArea {
 public:
  explicit SmoothScrollArea(QWidget* parent = nullptr);
  bool scrollForWheelEvent(const QWheelEvent& event);

 protected:
  void wheelEvent(QWheelEvent* event) override;

 private:
  QPropertyAnimation* scrollAnimation_ = nullptr;
};
