#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>

#include <memory>
#include <optional>

struct WindowsNativeWindow {
  quintptr id = 0;
  bool visible = false;
  bool cloaked = false;
  bool topLevel = false;
  QString title;
  QString className;
  QString executablePath;
  QSize clientSize;
};

class WindowsWindowApi {
 public:
  virtual ~WindowsWindowApi() = default;

  virtual QVector<WindowsNativeWindow> enumerateWindows() const = 0;
  virtual std::optional<WindowsNativeWindow> windowInfo(quintptr id) const = 0;
  virtual quintptr rootWindowAt(const QPoint& screenPoint) const = 0;
  virtual bool isWindow(quintptr id) const = 0;
  virtual quintptr foregroundWindow() const = 0;
  virtual bool activateWindow(quintptr id) = 0;
  virtual std::optional<QPoint> screenToClient(quintptr id,
                                               const QPoint& point) const = 0;
  virtual std::optional<QPoint> clientToScreen(quintptr id,
                                               const QPoint& point) const = 0;
  virtual QRect virtualDesktopRect() const = 0;
};

std::unique_ptr<WindowsWindowApi> createNativeWindowsWindowApi();
