#include "platform/PlatformServices.h"

#include "platform/windows/WindowsClickBackend.h"
#include "platform/windows/WindowsHotkeyService.h"

std::unique_ptr<ClickBackend> createClickBackend() {
  return std::make_unique<WindowsClickBackend>();
}

std::unique_ptr<HotkeyService> createHotkeyService() {
  return std::make_unique<WindowsHotkeyService>();
}
