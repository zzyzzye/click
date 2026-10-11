#pragma once

#include <QPoint>
#include <QRect>
#include <QVector>

#include "core/ClickTypes.h"

class ClickBackend {
 public:
  virtual ~ClickBackend() = default;

  virtual bool click(const ClickProfile& profile) = 0;
  virtual bool keyTap(const ClickProfile& profile) = 0;
  virtual QPoint currentCursorPosition() const = 0;
  virtual bool hasAccessibilityPermission() const = 0;
  virtual void requestAccessibilityPermission() = 0;
  virtual QVector<QRect> screenRects() const {
    return {};
  }
  virtual bool supportsKey(const QString&) const {
    return false;
  }
  virtual bool supportsText() const {
    return false;
  }
  virtual bool textInput(const QString&) {
    return false;
  }
};

