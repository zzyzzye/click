#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QList>

#include <memory>

#include "core/HotkeyService.h"
#include "platform/windows/WindowsHotkeyApi.h"

class WindowsHotkeyService final : public HotkeyService,
                                   public QAbstractNativeEventFilter {
  Q_OBJECT

 public:
  explicit WindowsHotkeyService(
      std::unique_ptr<WindowsHotkeyApi> api = createNativeWindowsHotkeyApi(),
      QObject* parent = nullptr);
  ~WindowsHotkeyService() override;

  bool registerHotkeys(const ClickProfile& profile) override;
  void unregisterAll() override;
  QString backendName() const override;

  bool nativeEventFilter(const QByteArray& eventType, void* message,
                         qintptr* result) override;

 private:
  enum class Action {
    StartStop = 1,
    CapturePoint = 2,
    EmergencyStop = 3,
    MacroRecord = 4,
    MacroPlayback = 5
  };

  bool registerOne(const QString& sequenceText, Action action);
  void dispatch(Action action);

  std::unique_ptr<WindowsHotkeyApi> api_;
  QList<int> activeIds_;
  QHash<int, Action> activeActions_;
};
