#include "platform/macos/MacOSHotkeyService.h"

#include <QKeySequence>

namespace {

constexpr OSType kHotkeySignature = 'CLKR';

}  // namespace

MacOSHotkeyService::MacOSHotkeyService(QObject* parent) : HotkeyService(parent) {
  EventTypeSpec eventType;
  eventType.eventClass = kEventClassKeyboard;
  eventType.eventKind = kEventHotKeyPressed;
  InstallApplicationEventHandler(&MacOSHotkeyService::handleEvent, 1, &eventType, this,
                                 &handlerRef_);
}

MacOSHotkeyService::~MacOSHotkeyService() {
  unregisterAll();
  if (handlerRef_) {
    RemoveEventHandler(handlerRef_);
  }
}

bool MacOSHotkeyService::registerHotkeys(const ClickProfile& profile) {
  unregisterAll();

  const bool ok = registerOne(profile.hotkeys.startStop, Action::StartStop) &&
                  registerOne(profile.hotkeys.capturePoint, Action::CapturePoint) &&
                  registerOne(profile.hotkeys.emergencyStop, Action::EmergencyStop);
  if (!ok) {
    unregisterAll();
    emit registrationFailed("Failed to register one or more global hotkeys.");
  }
  return ok;
}

void MacOSHotkeyService::unregisterAll() {
  for (EventHotKeyRef ref : hotkeyRefs_) {
    UnregisterEventHotKey(ref);
  }
  hotkeyRefs_.clear();
  registeredActions_.clear();
}

QString MacOSHotkeyService::backendName() const {
  return "Carbon EventHotKey";
}

OSStatus MacOSHotkeyService::handleEvent(EventHandlerCallRef, EventRef event, void* userData) {
  auto* self = static_cast<MacOSHotkeyService*>(userData);
  EventHotKeyID hotkeyId;
  GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr,
                    sizeof(hotkeyId), nullptr, &hotkeyId);
  self->dispatchHotkey(static_cast<int>(hotkeyId.id));
  return noErr;
}

bool MacOSHotkeyService::registerOne(const QString& sequenceText, Action action) {
  UInt32 modifiers = 0;
  UInt32 keyCode = 0;
  if (!parseSequence(sequenceText, &modifiers, &keyCode)) {
    return false;
  }

  EventHotKeyID hotkeyId;
  hotkeyId.signature = kHotkeySignature;
  hotkeyId.id = static_cast<UInt32>(action);

  EventHotKeyRef ref = nullptr;
  const OSStatus status = RegisterEventHotKey(keyCode, modifiers, hotkeyId,
                                              GetApplicationEventTarget(), 0, &ref);
  if (status != noErr || !ref) {
    return false;
  }

  hotkeyRefs_.append(ref);
  registeredActions_.insert(static_cast<int>(action), action);
  return true;
}

void MacOSHotkeyService::dispatchHotkey(int actionId) {
  if (!registeredActions_.contains(actionId)) {
    return;
  }
  const Action action = registeredActions_.value(actionId);
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

bool MacOSHotkeyService::parseSequence(const QString& sequenceText, UInt32* modifiers,
                                       UInt32* keyCode) const {
  const QKeySequence sequence =
      QKeySequence::fromString(sequenceText, QKeySequence::PortableText);
  if (sequence.count() < 1) {
    return false;
  }

  const QKeyCombination combo = sequence[0];
  *modifiers = carbonModifiers(combo.keyboardModifiers());
  return keyCodeForQtKey(combo.key(), keyCode);
}

UInt32 MacOSHotkeyService::carbonModifiers(Qt::KeyboardModifiers modifiers) const {
  UInt32 carbon = 0;
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    carbon |= shiftKey;
  }
  if (modifiers.testFlag(Qt::ControlModifier)) {
    carbon |= controlKey;
  }
  if (modifiers.testFlag(Qt::AltModifier)) {
    carbon |= optionKey;
  }
  if (modifiers.testFlag(Qt::MetaModifier)) {
    carbon |= cmdKey;
  }
  return carbon;
}

bool MacOSHotkeyService::keyCodeForQtKey(Qt::Key key, UInt32* keyCode) const {
  switch (key) {
    case Qt::Key_F1:
      *keyCode = kVK_F1;
      return true;
    case Qt::Key_F2:
      *keyCode = kVK_F2;
      return true;
    case Qt::Key_F3:
      *keyCode = kVK_F3;
      return true;
    case Qt::Key_F4:
      *keyCode = kVK_F4;
      return true;
    case Qt::Key_F5:
      *keyCode = kVK_F5;
      return true;
    case Qt::Key_F6:
      *keyCode = kVK_F6;
      return true;
    case Qt::Key_F7:
      *keyCode = kVK_F7;
      return true;
    case Qt::Key_F8:
      *keyCode = kVK_F8;
      return true;
    case Qt::Key_F9:
      *keyCode = kVK_F9;
      return true;
    case Qt::Key_F10:
      *keyCode = kVK_F10;
      return true;
    case Qt::Key_F11:
      *keyCode = kVK_F11;
      return true;
    case Qt::Key_F12:
      *keyCode = kVK_F12;
      return true;
    case Qt::Key_A:
      *keyCode = kVK_ANSI_A;
      return true;
    case Qt::Key_B:
      *keyCode = kVK_ANSI_B;
      return true;
    case Qt::Key_C:
      *keyCode = kVK_ANSI_C;
      return true;
    case Qt::Key_D:
      *keyCode = kVK_ANSI_D;
      return true;
    case Qt::Key_E:
      *keyCode = kVK_ANSI_E;
      return true;
    case Qt::Key_F:
      *keyCode = kVK_ANSI_F;
      return true;
    case Qt::Key_G:
      *keyCode = kVK_ANSI_G;
      return true;
    case Qt::Key_H:
      *keyCode = kVK_ANSI_H;
      return true;
    case Qt::Key_I:
      *keyCode = kVK_ANSI_I;
      return true;
    case Qt::Key_J:
      *keyCode = kVK_ANSI_J;
      return true;
    case Qt::Key_K:
      *keyCode = kVK_ANSI_K;
      return true;
    case Qt::Key_L:
      *keyCode = kVK_ANSI_L;
      return true;
    case Qt::Key_M:
      *keyCode = kVK_ANSI_M;
      return true;
    case Qt::Key_N:
      *keyCode = kVK_ANSI_N;
      return true;
    case Qt::Key_O:
      *keyCode = kVK_ANSI_O;
      return true;
    case Qt::Key_P:
      *keyCode = kVK_ANSI_P;
      return true;
    case Qt::Key_Q:
      *keyCode = kVK_ANSI_Q;
      return true;
    case Qt::Key_R:
      *keyCode = kVK_ANSI_R;
      return true;
    case Qt::Key_S:
      *keyCode = kVK_ANSI_S;
      return true;
    case Qt::Key_T:
      *keyCode = kVK_ANSI_T;
      return true;
    case Qt::Key_U:
      *keyCode = kVK_ANSI_U;
      return true;
    case Qt::Key_V:
      *keyCode = kVK_ANSI_V;
      return true;
    case Qt::Key_W:
      *keyCode = kVK_ANSI_W;
      return true;
    case Qt::Key_X:
      *keyCode = kVK_ANSI_X;
      return true;
    case Qt::Key_Y:
      *keyCode = kVK_ANSI_Y;
      return true;
    case Qt::Key_Z:
      *keyCode = kVK_ANSI_Z;
      return true;
    case Qt::Key_0:
      *keyCode = kVK_ANSI_0;
      return true;
    case Qt::Key_1:
      *keyCode = kVK_ANSI_1;
      return true;
    case Qt::Key_2:
      *keyCode = kVK_ANSI_2;
      return true;
    case Qt::Key_3:
      *keyCode = kVK_ANSI_3;
      return true;
    case Qt::Key_4:
      *keyCode = kVK_ANSI_4;
      return true;
    case Qt::Key_5:
      *keyCode = kVK_ANSI_5;
      return true;
    case Qt::Key_6:
      *keyCode = kVK_ANSI_6;
      return true;
    case Qt::Key_7:
      *keyCode = kVK_ANSI_7;
      return true;
    case Qt::Key_8:
      *keyCode = kVK_ANSI_8;
      return true;
    case Qt::Key_9:
      *keyCode = kVK_ANSI_9;
      return true;
    case Qt::Key_Space:
      *keyCode = kVK_Space;
      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      *keyCode = kVK_Return;
      return true;
    case Qt::Key_Escape:
      *keyCode = kVK_Escape;
      return true;
    default:
      return false;
  }
}
