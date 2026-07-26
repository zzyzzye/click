#include "platform/PlatformServices.h"

#include "platform/macos/MacOSClickBackend.h"
#include "platform/macos/MacOSHotkeyService.h"

std::unique_ptr<ClickBackend> createClickBackend() {
  return std::make_unique<MacOSClickBackend>();
}

std::unique_ptr<HotkeyService> createHotkeyService() {
  return std::make_unique<MacOSHotkeyService>();
}

