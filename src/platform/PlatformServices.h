#pragma once

#include <memory>

#include "core/MacroPlayer.h"
#include "core/MacroRecorder.h"
#include "core/WindowService.h"

class ClickBackend;
class HotkeyService;

std::unique_ptr<ClickBackend> createClickBackend();
std::unique_ptr<HotkeyService> createHotkeyService();

struct MacroPlatformServices {
  std::unique_ptr<WindowService> windowService;
  std::unique_ptr<MacroRecorder> recorder;
  std::unique_ptr<MacroPlayer> player;
};

MacroPlatformServices createMacroPlatformServices();

