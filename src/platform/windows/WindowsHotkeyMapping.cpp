#include "platform/windows/WindowsHotkeyMapping.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <QKeySequence>

namespace {

std::optional<quint32> virtualKeyForQtKey(Qt::Key key) {
  if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
    return quint32(VK_F1 + (key - Qt::Key_F1));
  }
  if (key >= Qt::Key_A && key <= Qt::Key_Z) {
    return quint32('A' + (key - Qt::Key_A));
  }
  if (key >= Qt::Key_0 && key <= Qt::Key_9) {
    return quint32('0' + (key - Qt::Key_0));
  }

  switch (key) {
    case Qt::Key_Space:
      return quint32(VK_SPACE);
    case Qt::Key_Return:
    case Qt::Key_Enter:
      return quint32(VK_RETURN);
    case Qt::Key_Escape:
      return quint32(VK_ESCAPE);
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<WindowsHotkeyBinding> mapWindowsHotkey(const QString& sequenceText) {
  const QKeySequence sequence =
      QKeySequence::fromString(sequenceText, QKeySequence::PortableText);
  if (sequence.count() != 1) {
    return std::nullopt;
  }

  const QKeyCombination combination = sequence[0];
  const auto virtualKey = virtualKeyForQtKey(combination.key());
  if (!virtualKey.has_value()) {
    return std::nullopt;
  }

  const Qt::KeyboardModifiers qtModifiers = combination.keyboardModifiers();
  const Qt::KeyboardModifiers supported =
      Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier |
      Qt::KeypadModifier;
  if ((qtModifiers & ~supported) != Qt::NoModifier) {
    return std::nullopt;
  }

  quint32 modifiers = MOD_NOREPEAT;
  if (qtModifiers.testFlag(Qt::ShiftModifier)) {
    modifiers |= MOD_SHIFT;
  }
  if (qtModifiers.testFlag(Qt::ControlModifier)) {
    modifiers |= MOD_CONTROL;
  }
  if (qtModifiers.testFlag(Qt::AltModifier)) {
    modifiers |= MOD_ALT;
  }
  if (qtModifiers.testFlag(Qt::MetaModifier)) {
    modifiers |= MOD_WIN;
  }

  return WindowsHotkeyBinding{modifiers, *virtualKey};
}
