#pragma once

#include "core/ClickBackend.h"

class MacOSClickBackend : public ClickBackend {
 public:
  bool click(const ClickProfile& profile) override;
  QPoint currentCursorPosition() const override;
  bool hasAccessibilityPermission() const override;
  void requestAccessibilityPermission() override;
};

