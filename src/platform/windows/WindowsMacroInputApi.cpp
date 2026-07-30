#include "platform/windows/WindowsMacroInputApi.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace {

class NativeWindowsMacroInputApi final : public WindowsMacroInputApi {
 public:
  bool send(const WindowsInjectedInput& value, quint32* errorCode) override {
    INPUT input{};
    if (value.kind == WindowsInjectedInputKind::Keyboard) {
      input.type = INPUT_KEYBOARD;
      input.ki.wVk = static_cast<WORD>(value.virtualKey);
      input.ki.wScan = static_cast<WORD>(value.scanCode);
      input.ki.dwFlags = value.flags;
      input.ki.dwExtraInfo = value.extraInfo;
    } else {
      input.type = INPUT_MOUSE;
      input.mi.dx = value.dx;
      input.mi.dy = value.dy;
      input.mi.mouseData = value.mouseData;
      input.mi.dwFlags = value.flags;
      input.mi.dwExtraInfo = value.extraInfo;
    }
    if (SendInput(1, &input, sizeof(INPUT)) == 1) return true;
    if (errorCode) *errorCode = GetLastError();
    return false;
  }
};

}  // namespace

std::unique_ptr<WindowsMacroInputApi> createNativeWindowsMacroInputApi() {
  return std::make_unique<NativeWindowsMacroInputApi>();
}
