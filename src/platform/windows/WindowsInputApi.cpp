#include "platform/windows/WindowsInputApi.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>

namespace {

class NativeWindowsInputApi final : public WindowsInputApi {
 public:
  std::optional<QPoint> cursorPosition() const override {
    POINT point{};
    if (!GetCursorPos(&point)) {
      return std::nullopt;
    }
    return QPoint(point.x, point.y);
  }

  bool clickAt(const QPoint& point, ClickButton button) override {
    if (!SetCursorPos(point.x(), point.y())) {
      return false;
    }

    const DWORD downFlag =
        button == ClickButton::Left ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    const DWORD upFlag =
        button == ClickButton::Left ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;

    std::array<INPUT, 2> inputs{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = downFlag;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = upFlag;

    return SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT)) ==
           inputs.size();
  }
};

}  // namespace

std::unique_ptr<WindowsInputApi> createNativeWindowsInputApi() {
  return std::make_unique<NativeWindowsInputApi>();
}
