#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>

struct WindowsHotkeyBinding {
  quint32 modifiers = 0;
  quint32 virtualKey = 0;
};

std::optional<WindowsHotkeyBinding> mapWindowsHotkey(const QString& sequenceText);
