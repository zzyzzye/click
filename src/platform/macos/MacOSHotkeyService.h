#pragma once

#include <Carbon/Carbon.h>

#include <QHash>

#include "core/HotkeyService.h"

class MacOSHotkeyService : public HotkeyService {
  Q_OBJECT

 public:
  explicit MacOSHotkeyService(QObject* parent = nullptr);
  ~MacOSHotkeyService() override;

  bool registerHotkeys(const ClickProfile& profile) override;
  void unregisterAll() override;
  QString backendName() const override;

 private:
  enum class Action {
    StartStop = 1,
    CapturePoint = 2,
    EmergencyStop = 3,
    MacroRecord = 4,
    MacroPlayback = 5
  };

  static OSStatus handleEvent(EventHandlerCallRef nextHandler, EventRef event,
                              void* userData);
  bool registerOne(const QString& sequenceText, Action action);
  void dispatchHotkey(int actionId);
  bool parseSequence(const QString& sequenceText, UInt32* modifiers, UInt32* keyCode) const;
  UInt32 carbonModifiers(Qt::KeyboardModifiers modifiers) const;
  bool keyCodeForQtKey(Qt::Key key, UInt32* keyCode) const;

  EventHandlerRef handlerRef_ = nullptr;
  QList<EventHotKeyRef> hotkeyRefs_;
  QHash<int, Action> registeredActions_;
};

