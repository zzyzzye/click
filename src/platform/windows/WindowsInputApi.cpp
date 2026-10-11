#include "platform/windows/WindowsInputApi.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>
#include <QVector>
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

    const bool ok =
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT)) == inputs.size();
    if (!ok)
      SendInput(1, &inputs[1], sizeof(INPUT));
    return ok;
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
    QVector<INPUT> inputs;
    QVector<WORD> modifiers;
    const auto mods = sequence[0].keyboardModifiers();
    if (mods.testFlag(Qt::ControlModifier))
      modifiers.append(VK_CONTROL);
    if (mods.testFlag(Qt::AltModifier))
      modifiers.append(VK_MENU);
    if (mods.testFlag(Qt::ShiftModifier))
      modifiers.append(VK_SHIFT);
    if (mods.testFlag(Qt::MetaModifier))
      modifiers.append(VK_LWIN);
    auto append = [&](WORD k, bool up) {
      INPUT input{};
      input.type = INPUT_KEYBOARD;
      input.ki.wVk = k;
      input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
      inputs.append(input);
    };
    for (WORD m : modifiers)
      append(m, false);
    append(virtualKey, false);
    append(virtualKey, true);
    for (auto it = modifiers.crbegin(); it != modifiers.crend(); ++it)
      append(*it, true);
    const bool ok = SendInput(UINT(inputs.size()), inputs.data(), sizeof(INPUT)) == inputs.size();
    if (!ok) {
      // 部分发送也必须尝试释放所有模拟按键。
      inputs.clear();
      append(virtualKey, true);
      for (auto it = modifiers.crbegin(); it != modifiers.crend(); ++it)
        append(*it, true);
      SendInput(UINT(inputs.size()), inputs.data(), sizeof(INPUT));
    }
    return ok;
  }
  bool textInput(const QString& text) override {
    QVector<INPUT> inputs;
    for (QChar c : text) {
      INPUT down{};
      down.type = INPUT_KEYBOARD;
      down.ki.wScan = c.unicode();
      down.ki.dwFlags = KEYEVENTF_UNICODE;
      inputs.append(down);
      down.ki.dwFlags |= KEYEVENTF_KEYUP;
      inputs.append(down);
    }
    const bool ok = SendInput(UINT(inputs.size()), inputs.data(), sizeof(INPUT)) == inputs.size();
    if (!ok) {
      for (auto& input : inputs)
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
      SendInput(UINT(inputs.size()), inputs.data(), sizeof(INPUT));
    }
    return ok;
  }
};

}  // namespace

std::unique_ptr<WindowsInputApi> createNativeWindowsInputApi() {
  return std::make_unique<NativeWindowsInputApi>();
}
