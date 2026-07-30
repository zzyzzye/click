#include "platform/windows/WindowsInputApi.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>
#include <QKeySequence>

namespace {

class NativeWindowsInputApi final : public WindowsInputApi {
 public:
  std::optional<QPoint> cursorPosition() const override {
    POINT point{};
    if (!GetCursorPos(&point)) {
      return std::nullopt;
    }
    return QPoint(point.x, point.y);
  }

  bool clickAt(const QPoint& point, ClickButton button) override {
    if (!SetCursorPos(point.x(), point.y())) {
      return false;
    }

    const DWORD downFlag =
        button == ClickButton::Left ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    const DWORD upFlag =
        button == ClickButton::Left ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;

    std::array<INPUT, 2> inputs{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = downFlag;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = upFlag;

    return SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT)) ==
           inputs.size();
  }

  bool keyTap(const QString& keyText) override {
    const QKeySequence sequence = QKeySequence::fromString(keyText, QKeySequence::PortableText);
    if (sequence.count() != 1) return false;
    const int key = sequence[0].toCombined() & ~Qt::KeyboardModifierMask;
    WORD virtualKey = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) virtualKey = WORD('A' + key - Qt::Key_A);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9) virtualKey = WORD('0' + key - Qt::Key_0);
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) virtualKey = WORD(VK_F1 + key - Qt::Key_F1);
    else if (key == Qt::Key_Space) virtualKey = VK_SPACE;
    else if (key == Qt::Key_Return || key == Qt::Key_Enter) virtualKey = VK_RETURN;
    else if (key == Qt::Key_Tab) virtualKey = VK_TAB;
    else if (key == Qt::Key_Escape) virtualKey = VK_ESCAPE;
    if (!virtualKey) return false;
    std::array<INPUT, 2> inputs{};
    inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = virtualKey;
    inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = virtualKey; inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT)) == inputs.size();
  }
};

}  // namespace

std::unique_ptr<WindowsInputApi> createNativeWindowsInputApi() {
  return std::make_unique<NativeWindowsInputApi>();
}
