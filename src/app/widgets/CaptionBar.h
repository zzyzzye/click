#pragma once

#include <QAbstractButton>
#include <QWidget>

class QLabel;

// 沉浸式标题栏：左侧窗口标题，右侧最小化/最大化/关闭按钮，
// 空白区域拖动窗口、双击切换最大化。配合 MainWindow 的 WM_NCCALCSIZE 使用。
class CaptionBar final : public QWidget {
  Q_OBJECT
 public:
  explicit CaptionBar(const QString& title, QWidget* parent = nullptr);

  void setMaximized(bool maximized);
  bool isMaximized() const { return maximized_; }

  QSize sizeHint() const override { return QSize(-1, 32); }

 signals:
  void minimizeRequested();
  void maximizeRestoreRequested();
  void closeRequested();

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;

 private:
  QLabel* titleLabel_ = nullptr;
  QAbstractButton* minimizeButton_ = nullptr;
  QAbstractButton* maximizeButton_ = nullptr;
  QAbstractButton* closeButton_ = nullptr;
  bool maximized_ = false;
};
