#pragma once

#include <QPoint>

#include <functional>
#include <memory>

#include "core/MacroTypes.h"

inline constexpr quintptr kClickFlowInjectedInputMarker =
    sizeof(quintptr) == 8 ? quintptr(0x43464c4f574d4143ULL) : quintptr(0x574d4143UL);

enum class WindowsRawInputType {
  KeyDown,
  KeyUp,
  MouseMove,
  MouseButtonDown,
  MouseButtonUp,
  Wheel,
  HorizontalWheel
};

struct WindowsRawInput {
  WindowsRawInputType type = WindowsRawInputType::MouseMove;
  qint64 timestampUs = 0;
  quint32 virtualKey = 0;
  quint32 scanCode = 0;
  quint32 flags = 0;
  QPoint screenPoint;
  MacroMouseButton button = MacroMouseButton::None;
  int wheelDelta = 0;
  quintptr extraInfo = 0;
};

class WindowsMacroHookApi {
 public:
  using InputCallback = std::function<void(const WindowsRawInput&)>;

  virtual ~WindowsMacroHookApi() = default;
  virtual bool start(InputCallback callback, QString* error = nullptr) = 0;
  virtual void stop() = 0;
  virtual bool isRunning() const = 0;
  virtual qint64 monotonicNowUs() const = 0;
};

std::unique_ptr<WindowsMacroHookApi> createNativeWindowsMacroHookApi();
