#include "platform/PlatformServices.h"

#include "platform/windows/WindowsClickBackend.h"
#include "platform/windows/WindowsHotkeyService.h"
#include "platform/windows/WindowsMacroPlayer.h"
#include "platform/windows/WindowsMacroRecorder.h"
#include "platform/windows/WindowsWindowService.h"

std::unique_ptr<ClickBackend> createClickBackend() {
  return std::make_unique<WindowsClickBackend>();
}

std::unique_ptr<HotkeyService> createHotkeyService() {
  return std::make_unique<WindowsHotkeyService>();
}

MacroPlatformServices createMacroPlatformServices() {
  MacroPlatformServices services;
  services.windowService = std::make_unique<WindowsWindowService>();
  services.recorder =
      std::make_unique<WindowsMacroRecorder>(createNativeWindowsMacroHookApi(),
                                             services.windowService.get());
  services.player =
      std::make_unique<WindowsMacroPlayer>(createNativeWindowsMacroInputApi(),
                                           services.windowService.get());
  return services;
}
