#pragma once

#include <memory>

#include "platform/WindowStyleService.h"

class QWidget;

// 纯函数，便于单元测试。传入注册表读到的 CurrentBuildNumber。
bool isWindows11OrLater(unsigned int buildNumber);

class WindowsWindowStyle final : public WindowStyleService {
 public:
  void prepare(QWidget* window) override;
  void apply(QWidget* window) override;
  bool usesBackdrop() const override;
  bool prefersDarkTheme() const override;
};
