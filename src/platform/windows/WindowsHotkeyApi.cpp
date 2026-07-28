#include "platform/windows/WindowsHotkeyApi.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace {

class NativeWindowsHotkeyApi final : public WindowsHotkeyApi {
 public:
  bool registerHotkey(int id, quint32 modifiers, quint32 virtualKey) override {
    return RegisterHotKey(nullptr, id, modifiers, virtualKey) != FALSE;
  }

  void unregisterHotkey(int id) override {
    UnregisterHotKey(nullptr, id);
  }
};

}  // namespace

std::unique_ptr<WindowsHotkeyApi> createNativeWindowsHotkeyApi() {
  return std::make_unique<NativeWindowsHotkeyApi>();
}
