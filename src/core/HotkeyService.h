#pragma once

#include <QObject>

#include "core/ClickTypes.h"

class HotkeyService : public QObject {
  Q_OBJECT

 public:
  explicit HotkeyService(QObject* parent = nullptr) : QObject(parent) {}
  ~HotkeyService() override = default;

  virtual bool registerHotkeys(const ClickProfile& profile) = 0;
  virtual void unregisterAll() = 0;
  virtual QString backendName() const = 0;

 signals:
  void startStopPressed();
  void capturePointPressed();
  void emergencyStopPressed();
  void macroRecordPressed();
  void macroPlaybackPressed();
  void registrationFailed(const QString& message);
};

