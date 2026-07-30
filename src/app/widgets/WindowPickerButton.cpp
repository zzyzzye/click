#include "app/widgets/WindowPickerButton.h"

#include <QCursor>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>

WindowPickerButton::WindowPickerButton(QWidget* parent) : QPushButton(parent) {
  setText("拖动准星选窗");
  setObjectName("macroWindowPickerButton");
  setCursor(Qt::CrossCursor);
}

void WindowPickerButton::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QPushButton::mousePressEvent(event);
    return;
  }
  picking_ = true;
  setText("释放以选择窗口");
  grabMouse(Qt::CrossCursor);
  event->accept();
}

void WindowPickerButton::mouseReleaseEvent(QMouseEvent* event) {
  if (!picking_ || event->button() != Qt::LeftButton) {
    QPushButton::mouseReleaseEvent(event);
    return;
  }
  const QPoint point = QCursor::pos();
  cancelPicking();
  emit windowPointSelected(point);
  event->accept();
}

void WindowPickerButton::focusOutEvent(QFocusEvent* event) {
  cancelPicking();
  QPushButton::focusOutEvent(event);
}

void WindowPickerButton::keyPressEvent(QKeyEvent* event) {
  if (picking_ && event->key() == Qt::Key_Escape) {
    cancelPicking();
    event->accept();
    return;
  }
  QPushButton::keyPressEvent(event);
}

void WindowPickerButton::cancelPicking() {
  if (!picking_) return;
  picking_ = false;
  releaseMouse();
  setText("拖动准星选窗");
}
