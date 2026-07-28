#pragma once

#include <QtGlobal>

#include <memory>

class WindowsHotkeyApi {
 public:
  virtual ~WindowsHotkeyApi() = default;

  virtual bool registerHotkey(int id, quint32 modifiers, quint32 virtualKey) = 0;
  virtual void unregisterHotkey(int id) = 0;
};

std::unique_ptr<WindowsHotkeyApi> createNativeWindowsHotkeyApi();
