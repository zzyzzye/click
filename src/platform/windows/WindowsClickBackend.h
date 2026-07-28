#pragma once

#include <functional>
#include <memory>

#include "core/ClickBackend.h"
#include "platform/windows/WindowsInputApi.h"

using RandomOffset = std::function<int(int minimum, int maximumInclusive)>;

int defaultWindowsRandomOffset(int minimum, int maximumInclusive);

class WindowsClickBackend final : public ClickBackend {
 public:
  explicit WindowsClickBackend(
      std::unique_ptr<WindowsInputApi> api = createNativeWindowsInputApi(),
      RandomOffset randomOffset = defaultWindowsRandomOffset);

  bool click(const ClickProfile& profile) override;
  QPoint currentCursorPosition() const override;
  bool hasAccessibilityPermission() const override;
  void requestAccessibilityPermission() override;

 private:
  std::unique_ptr<WindowsInputApi> api_;
  RandomOffset randomOffset_;
};
