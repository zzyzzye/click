#include "platform/windows/WindowsHotkeyService.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <QCoreApplication>

#include <utility>

#include "platform/windows/WindowsHotkeyMapping.h"

WindowsHotkeyService::WindowsHotkeyService(std::unique_ptr<WindowsHotkeyApi> api,
                                           QObject* parent)
    : HotkeyService(parent), api_(std::move(api)) {
  if (QCoreApplication::instance()) {
    QCoreApplication::instance()->installNativeEventFilter(this);
  }
}

WindowsHotkeyService::~WindowsHotkeyService() {
  unregisterAll();
  if (QCoreApplication::instance()) {
    QCoreApplication::instance()->removeNativeEventFilter(this);
  }
}

bool WindowsHotkeyService::registerHotkeys(const ClickProfile& profile) {
  unregisterAll();

  const struct {
    QString sequence;
    Action action;
  } bindings[] = {
      {profile.hotkeys.startStop, Action::StartStop},
      {profile.hotkeys.capturePoint, Action::CapturePoint},
      {profile.hotkeys.emergencyStop, Action::EmergencyStop},
  };

  for (const auto& binding : bindings) {
    if (!registerOne(binding.sequence, binding.action)) {
      unregisterAll();
      return false;
    }
  }
  return true;
}

void WindowsHotkeyService::unregisterAll() {
  if (api_) {
    for (int id : activeIds_) {
      api_->unregisterHotkey(id);
    }
  }
  activeIds_.clear();
  activeActions_.clear();
}

QString WindowsHotkeyService::backendName() const {
  return "Win32 RegisterHotKey";
}

bool WindowsHotkeyService::nativeEventFilter(const QByteArray&, void* message,
                                             qintptr*) {
  if (!message) {
    return false;
  }

  const auto* nativeMessage = static_cast<MSG*>(message);
  if (nativeMessage->message != WM_HOTKEY) {
    return false;
  }

  const int id = static_cast<int>(nativeMessage->wParam);
  const auto action = activeActions_.constFind(id);
  if (action != activeActions_.cend()) {
    dispatch(*action);
  }
  return false;
}

bool WindowsHotkeyService::registerOne(const QString& sequenceText, Action action) {
  const auto binding = mapWindowsHotkey(sequenceText);
  if (!binding.has_value()) {
    emit registrationFailed(QString("不支持的全局热键：%1").arg(sequenceText));
    return false;
  }

  const int id = static_cast<int>(action);
  if (!api_ || !api_->registerHotkey(id, binding->modifiers, binding->virtualKey)) {
    emit registrationFailed(
        QString("无法注册全局热键 %1，可能已被其他应用占用。").arg(sequenceText));
    return false;
  }

  activeIds_.append(id);
  activeActions_.insert(id, action);
  return true;
}

void WindowsHotkeyService::dispatch(Action action) {
  switch (action) {
    case Action::StartStop:
      emit startStopPressed();
      break;
    case Action::CapturePoint:
      emit capturePointPressed();
      break;
    case Action::EmergencyStop:
      emit emergencyStopPressed();
      break;
  }
}
