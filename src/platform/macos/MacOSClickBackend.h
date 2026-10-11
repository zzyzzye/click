#pragma once

#include "core/ClickBackend.h"

class MacOSClickBackend : public ClickBackend {
 public:
  bool click(const ClickProfile& profile) override;
  bool keyTap(const ClickProfile& profile) override;
  QPoint currentCursorPosition() const override;
  bool hasAccessibilityPermission() const override;
  void requestAccessibilityPermission() override;
  QVector<QRect> screenRects() const override;
  bool supportsKey(const QString& key) const override;
  bool supportsText() const override {
    return true;
  }
  bool textInput(const QString& text) override;
};

