#pragma once

#include <memory>

#include "core/WindowService.h"
#include "platform/windows/WindowsWindowApi.h"

class WindowsWindowService final : public WindowService {
 public:
  explicit WindowsWindowService(
      std::unique_ptr<WindowsWindowApi> api = createNativeWindowsWindowApi(),
      quintptr excludedWindow = 0);

  QVector<WindowTarget> availableWindows() const override;
  std::optional<WindowTarget> windowAt(const QPoint& screenPoint) const override;
  std::optional<WindowTarget> resolve(const WindowTarget& target,
                                      QString* error = nullptr) const override;
  bool isAlive(const WindowTarget& target) const override;
  bool isForeground(const WindowTarget& target) const override;
  bool activate(const WindowTarget& target) override;
  QSize clientSize(const WindowTarget& target) const override;
  std::optional<QPoint> screenToClient(const WindowTarget& target,
                                       const QPoint& point) const override;
  std::optional<QPoint> clientToScreen(const WindowTarget& target,
                                       const QPoint& point) const override;
  QRect virtualDesktopRect() const override;
  QString displayName(const WindowTarget& target) const override;

 private:
  bool eligible(const WindowsNativeWindow& window) const;
  WindowTarget toTarget(const WindowsNativeWindow& window) const;

  std::unique_ptr<WindowsWindowApi> api_;
  quintptr excludedWindow_ = 0;
};
