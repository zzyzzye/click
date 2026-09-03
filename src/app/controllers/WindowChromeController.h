#pragma once

#include <QByteArray>

class CaptionBar;
class QEvent;
class QMainWindow;
class WindowStyleService;

// 主窗口 Chrome 协调器：集中管理自绘标题栏、窗口状态和原生窗口消息。
class WindowChromeController final {
 public:
  WindowChromeController(QMainWindow* window, CaptionBar* captionBar,
                         WindowStyleService* windowStyle);

  void handleShown();
  void handleWindowStateChange(const QEvent* event);
  void setAlwaysOnTop(bool enabled);
  bool handleNativeEvent(const QByteArray& eventType, void* message,
                         qintptr* result);

 private:
  QMainWindow* window_ = nullptr;
  CaptionBar* captionBar_ = nullptr;
  WindowStyleService* windowStyle_ = nullptr;
};
