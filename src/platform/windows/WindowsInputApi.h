#pragma once

#include <QPoint>

#include <memory>
#include <optional>

#include "core/ClickTypes.h"

class WindowsInputApi {
 public:
  virtual ~WindowsInputApi() = default;

  virtual std::optional<QPoint> cursorPosition() const = 0;
  virtual bool clickAt(const QPoint& point, ClickButton button) = 0;
  virtual bool keyTap(const QString& keyText) = 0;
};

std::unique_ptr<WindowsInputApi> createNativeWindowsInputApi();
