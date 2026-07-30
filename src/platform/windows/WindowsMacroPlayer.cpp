#include "platform/windows/WindowsMacroPlayer.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>

#include "platform/windows/WindowsMacroHookApi.h"

namespace {

bool isKeyboardEvent(MacroEventType type) {
  return type == MacroEventType::KeyDown || type == MacroEventType::KeyUp;
}

bool isMouseEvent(MacroEventType type) {
  return !isKeyboardEvent(type);
}

bool isExtended(const MacroEvent& event) {
  return (event.nativeFlags & 0x01U) != 0;
}

}  // namespace

WindowsMacroPlayer::WindowsMacroPlayer(
    std::unique_ptr<WindowsMacroInputApi> inputApi,
    WindowService* windowService, QObject* parent)
    : MacroPlayer(parent),
      inputApi_(std::move(inputApi)),
      windowService_(windowService) {}

WindowsMacroPlayer::~WindowsMacroPlayer() {
  cancel();
}

bool WindowsMacroPlayer::prepare(const MacroSequence& sequence, QString* error) {
  cancel();
  if (!inputApi_ || !windowService_) {
    if (error) *error = "Windows 宏回放服务不可用。";
    return false;
  }
  if (sequence.events.isEmpty()) {
    if (error) *error = "宏没有可回放的事件。";
    return false;
  }

  targetMode_ = sequence.targetMode;
  if (targetMode_ == MacroTargetMode::Window) {
    QString resolveError;
    const auto resolved = windowService_->resolve(sequence.target, &resolveError);
    if (!resolved) {
      if (error) *error = resolveError;
      return false;
    }
    target_ = *resolved;
    recordedClientSize_ = sequence.target.clientSize;
    if (!WindowService::sizeMatches(recordedClientSize_,
                                    windowService_->clientSize(target_))) {
      if (error) *error = "目标窗口尺寸与录制时不一致，请恢复窗口尺寸后重试。";
      return false;
    }
    if (!windowService_->isForeground(target_) && !windowService_->activate(target_)) {
      if (error) *error = "无法激活目标窗口，请手动切换到该窗口后重试。";
      return false;
    }
  } else {
    const QRect desktop = windowService_->virtualDesktopRect();
    if (!desktop.isValid() || desktop.isEmpty()) {
      if (error) *error = "无法读取当前显示器布局。";
      return false;
    }
    for (const auto& event : sequence.events) {
      if (isMouseEvent(event.type) && !desktop.contains(event.point)) {
        if (error) *error = "宏坐标超出当前显示器布局，无法安全回放。";
        return false;
      }
    }
  }
  prepared_ = true;
  return true;
}

bool WindowsMacroPlayer::inject(const MacroEvent& event, QString* error) {
  if (!prepared_) {
    if (error) *error = "宏回放尚未准备完成。";
    return false;
  }

  WindowsInjectedInput input;
  if (isKeyboardEvent(event.type)) {
    if (targetMode_ == MacroTargetMode::Window) {
      if (!windowService_->isAlive(target_) || !windowService_->isForeground(target_) ||
          !WindowService::sizeMatches(recordedClientSize_,
                                      windowService_->clientSize(target_))) {
        if (error) *error = "目标窗口已关闭、失焦或尺寸发生变化，回放已停止。";
        return false;
      }
    }
    input = keyboardInput(event);
  } else {
    const auto point = screenPointFor(event, error);
    if (!point) return false;
    input = mouseInput(event, *point);
  }

  if (!send(input, error)) return false;
  updateHeldState(event);
  return true;
}

void WindowsMacroPlayer::releaseAll() {
  if (!inputApi_) {
    heldButtons_.clear();
    heldKeys_.clear();
    return;
  }

  for (auto iterator = heldButtons_.crbegin(); iterator != heldButtons_.crend();
       ++iterator) {
    WindowsInjectedInput input;
    input.kind = WindowsInjectedInputKind::Mouse;
    input.flags = mouseButtonFlag(*iterator, false);
    if (*iterator == MacroMouseButton::X1) input.mouseData = XBUTTON1;
    if (*iterator == MacroMouseButton::X2) input.mouseData = XBUTTON2;
    input.extraInfo = kClickFlowInjectedInputMarker;
    inputApi_->send(input);
  }
  for (auto iterator = heldKeys_.crbegin(); iterator != heldKeys_.crend(); ++iterator) {
    WindowsInjectedInput input;
    input.kind = WindowsInjectedInputKind::Keyboard;
    input.virtualKey = iterator->scanCode ? 0 : iterator->virtualKey;
    input.scanCode = iterator->scanCode;
    input.flags = KEYEVENTF_KEYUP;
    if (iterator->scanCode) input.flags |= KEYEVENTF_SCANCODE;
    if (iterator->extended) input.flags |= KEYEVENTF_EXTENDEDKEY;
    input.extraInfo = kClickFlowInjectedInputMarker;
    inputApi_->send(input);
  }
  heldButtons_.clear();
  heldKeys_.clear();
}

void WindowsMacroPlayer::cancel() {
  releaseAll();
  prepared_ = false;
  target_ = {};
  recordedClientSize_ = {};
}

bool WindowsMacroPlayer::send(const WindowsInjectedInput& input, QString* error) {
  quint32 errorCode = 0;
  if (inputApi_ && inputApi_->send(input, &errorCode)) return true;
  if (error) {
    *error = QString("输入注入失败（错误 %1），目标程序可能具有更高权限。")
                 .arg(errorCode);
  }
  return false;
}

std::optional<QPoint> WindowsMacroPlayer::screenPointFor(
    const MacroEvent& event, QString* error) const {
  QPoint point = event.point;
  if (targetMode_ == MacroTargetMode::Window) {
    if (!windowService_->isAlive(target_) || !windowService_->isForeground(target_) ||
        !WindowService::sizeMatches(recordedClientSize_,
                                    windowService_->clientSize(target_))) {
      if (error) *error = "目标窗口已关闭、失焦或尺寸发生变化，回放已停止。";
      return std::nullopt;
    }
    const auto converted = windowService_->clientToScreen(target_, point);
    if (!converted) {
      if (error) *error = "无法转换目标窗口坐标。";
      return std::nullopt;
    }
    point = *converted;
  }
  if (!windowService_->virtualDesktopRect().contains(point)) {
    if (error) *error = "宏坐标超出当前显示器布局。";
    return std::nullopt;
  }
  return point;
}

WindowsInjectedInput WindowsMacroPlayer::keyboardInput(
    const MacroEvent& event) const {
  WindowsInjectedInput input;
  input.kind = WindowsInjectedInputKind::Keyboard;
  input.virtualKey = event.scanCode ? 0 : event.virtualKey;
  input.scanCode = event.scanCode;
  if (event.scanCode) input.flags |= KEYEVENTF_SCANCODE;
  if (event.type == MacroEventType::KeyUp) input.flags |= KEYEVENTF_KEYUP;
  if (isExtended(event)) input.flags |= KEYEVENTF_EXTENDEDKEY;
  input.extraInfo = kClickFlowInjectedInputMarker;
  return input;
}

WindowsInjectedInput WindowsMacroPlayer::mouseInput(
    const MacroEvent& event, const QPoint& screenPoint) const {
  WindowsInjectedInput input;
  input.kind = WindowsInjectedInputKind::Mouse;
  const QRect desktop = windowService_->virtualDesktopRect();
  input.dx = static_cast<qint32>(
      qint64(screenPoint.x() - desktop.left()) * 65535 /
      std::max(1, desktop.width() - 1));
  input.dy = static_cast<qint32>(
      qint64(screenPoint.y() - desktop.top()) * 65535 /
      std::max(1, desktop.height() - 1));
  input.flags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  if (event.type == MacroEventType::MouseButtonDown) {
    input.flags |= mouseButtonFlag(event.button, true);
  } else if (event.type == MacroEventType::MouseButtonUp) {
    input.flags |= mouseButtonFlag(event.button, false);
  } else if (event.type == MacroEventType::Wheel) {
    input.flags |= MOUSEEVENTF_WHEEL;
    input.mouseData = static_cast<quint32>(event.wheelDelta);
  } else if (event.type == MacroEventType::HorizontalWheel) {
    input.flags |= MOUSEEVENTF_HWHEEL;
    input.mouseData = static_cast<quint32>(event.wheelDelta);
  }
  if (event.button == MacroMouseButton::X1) input.mouseData = XBUTTON1;
  if (event.button == MacroMouseButton::X2) input.mouseData = XBUTTON2;
  input.extraInfo = kClickFlowInjectedInputMarker;
  return input;
}

quint32 WindowsMacroPlayer::mouseButtonFlag(MacroMouseButton button,
                                             bool down) const {
  switch (button) {
    case MacroMouseButton::Left: return down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    case MacroMouseButton::Right: return down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    case MacroMouseButton::Middle:
      return down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    case MacroMouseButton::X1:
    case MacroMouseButton::X2: return down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    case MacroMouseButton::None: return 0;
  }
  return 0;
}

void WindowsMacroPlayer::updateHeldState(const MacroEvent& event) {
  if (event.type == MacroEventType::KeyDown) {
    const HeldKey key{event.virtualKey, event.scanCode, isExtended(event)};
    const bool exists = std::any_of(heldKeys_.cbegin(), heldKeys_.cend(),
                                    [&key](const HeldKey& held) {
                                      return held.virtualKey == key.virtualKey &&
                                             held.scanCode == key.scanCode &&
                                             held.extended == key.extended;
                                    });
    if (!exists) heldKeys_.append(key);
  } else if (event.type == MacroEventType::KeyUp) {
    heldKeys_.erase(std::remove_if(heldKeys_.begin(), heldKeys_.end(),
                                   [&event](const HeldKey& held) {
                                     return held.virtualKey == event.virtualKey &&
                                            held.scanCode == event.scanCode &&
                                            held.extended == isExtended(event);
                                   }),
                    heldKeys_.end());
  } else if (event.type == MacroEventType::MouseButtonDown) {
    if (!heldButtons_.contains(event.button)) heldButtons_.append(event.button);
  } else if (event.type == MacroEventType::MouseButtonUp) {
    heldButtons_.removeAll(event.button);
  }
}
