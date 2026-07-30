#pragma once

#include <QVector>

#include <memory>

#include "core/MacroPlayer.h"
#include "core/WindowService.h"
#include "platform/windows/WindowsMacroInputApi.h"

class WindowsMacroPlayer final : public MacroPlayer {
  Q_OBJECT

 public:
  explicit WindowsMacroPlayer(
      std::unique_ptr<WindowsMacroInputApi> inputApi =
          createNativeWindowsMacroInputApi(),
      WindowService* windowService = nullptr, QObject* parent = nullptr);
  ~WindowsMacroPlayer() override;

  bool prepare(const MacroSequence& sequence, QString* error = nullptr) override;
  bool inject(const MacroEvent& event, QString* error = nullptr) override;
  void releaseAll() override;
  void cancel() override;

 private:
  struct HeldKey {
    quint32 virtualKey = 0;
    quint32 scanCode = 0;
    bool extended = false;
  };

  bool send(const WindowsInjectedInput& input, QString* error);
  std::optional<QPoint> screenPointFor(const MacroEvent& event,
                                       QString* error) const;
  WindowsInjectedInput keyboardInput(const MacroEvent& event) const;
  WindowsInjectedInput mouseInput(const MacroEvent& event,
                                  const QPoint& screenPoint) const;
  quint32 mouseButtonFlag(MacroMouseButton button, bool down) const;
  void updateHeldState(const MacroEvent& event);

  std::unique_ptr<WindowsMacroInputApi> inputApi_;
  WindowService* windowService_ = nullptr;
  MacroTargetMode targetMode_ = MacroTargetMode::Global;
  WindowTarget target_;
  QSize recordedClientSize_;
  QVector<HeldKey> heldKeys_;
  QVector<MacroMouseButton> heldButtons_;
  bool prepared_ = false;
};
