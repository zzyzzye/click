#pragma once

#include <QPushButton>

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;

class WindowPickerButton final : public QPushButton {
  Q_OBJECT

 public:
  explicit WindowPickerButton(QWidget* parent = nullptr);

 signals:
  void windowPointSelected(const QPoint& globalPoint);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void cancelPicking();

  bool picking_ = false;
};
