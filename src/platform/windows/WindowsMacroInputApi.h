#pragma once

#include <QVector>

#include <memory>

enum class WindowsInjectedInputKind {
  Keyboard,
  Mouse
};

struct WindowsInjectedInput {
  WindowsInjectedInputKind kind = WindowsInjectedInputKind::Keyboard;
  quint32 virtualKey = 0;
  quint32 scanCode = 0;
  quint32 flags = 0;
  qint32 dx = 0;
  qint32 dy = 0;
  quint32 mouseData = 0;
  quintptr extraInfo = 0;
};

class WindowsMacroInputApi {
 public:
  virtual ~WindowsMacroInputApi() = default;
  virtual bool send(const WindowsInjectedInput& input,
                    quint32* errorCode = nullptr) = 0;
};

std::unique_ptr<WindowsMacroInputApi> createNativeWindowsMacroInputApi();
