#pragma once

#include <memory>

#include <QtGlobal>

#include "platform/WindowStyleService.h"

class QWidget;

// 纯函数，便于单元测试。传入注册表读到的 CurrentBuildNumber。
bool isWindows11OrLater(unsigned int buildNumber);

// DWMWA_SYSTEMBACKDROP_TYPE（Mica）自 Windows 11 22H2（build 22621）起可用。
bool supportsSystemBackdrop(unsigned int buildNumber);

// Mica 生效的全部条件：Win11、build 支持 backdrop、系统「透明效果」开启。
bool micaBackdropAvailable(unsigned int buildNumber, bool transparencyEnabled);

// 去掉 Windows 原生标题栏绘制，同时保留缩放、系统菜单和窗口控制能力。
quintptr clickFlowNativeWindowStyle(quintptr currentStyle);

class WindowsWindowStyle final : public WindowStyleService {
 public:
  void prepare(QWidget* window) override;
  void apply(QWidget* window) override;
  bool usesBackdrop() const override;
};
