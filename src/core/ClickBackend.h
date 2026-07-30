#pragma once

#include <QPoint>

#include "core/ClickTypes.h"

class ClickBackend {
 public:
  virtual ~ClickBackend() = default;

  virtual bool click(const ClickProfile& profile) = 0;
  virtual bool keyTap(const ClickProfile& profile) = 0;
  virtual QPoint currentCursorPosition() const = 0;
  virtual bool hasAccessibilityPermission() const = 0;
  virtual void requestAccessibilityPermission() = 0;
};

