#include "platform/PlatformServices.h"

#include "platform/WindowStyleService.h"
#include "platform/macos/MacOSClickBackend.h"
#include "platform/macos/MacOSHotkeyService.h"

std::unique_ptr<ClickBackend> createClickBackend() {
  return std::make_unique<MacOSClickBackend>();
}

std::unique_ptr<HotkeyService> createHotkeyService() {
  return std::make_unique<MacOSHotkeyService>();
}

MacroPlatformServices createMacroPlatformServices() {
  return {};
}

namespace {
class NullWindowStyleService final : public WindowStyleService {
 public:
  void prepare(QWidget*) override {}
  void apply(QWidget*) override {}
  bool usesBackdrop() const override { return false; }
  bool prefersDarkTheme() const override { return false; }
};
}  // namespace

std::unique_ptr<WindowStyleService> createWindowStyleService() {
  return std::make_unique<NullWindowStyleService>();
}

