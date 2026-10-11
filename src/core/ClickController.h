#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

#include "core/ClickBackend.h"

class AutomationCoordinator;

class ClickController : public QObject {
  Q_OBJECT

 public:
  explicit ClickController(ClickBackend* backend, QObject* parent = nullptr);
  ClickController(ClickBackend* backend, AutomationCoordinator* coordinator,
                  QObject* parent = nullptr);

  void start(const ClickProfile& profile);
  void stop();
  void emergencyStop();

  bool isRunning() const;
  int remainingClicks() const;
  int clicksExecuted() const;
  QString currentStatus() const;

 signals:
  void statusChanged(const QString& status);
  void runningChanged(bool running);
  void remainingClicksChanged(int remaining);
  void clicksExecutedChanged(int executed);
  void countdownChanged(int remainingSeconds);
  void startRejected(const QString& reason);

 private slots:
  void handleCountdownTick();
  void handleClickTick();

 private:
  enum class State {
    Idle,
    Countdown,
    Running,
    Completed,
    PermissionDenied,
    Error
  };

  void beginRun();
  void performClick();
  bool validateInput(const ClickProfile& profile, QString* error, bool checkLayout) const;
  void finishRun(State state, const QString& status);
  void setStatus(const QString& status);

  ClickBackend* backend_;
  AutomationCoordinator* coordinator_ = nullptr;
  QTimer clickTimer_;
  QTimer countdownTimer_;
  QElapsedTimer validationClock_;
  QVector<QRect> activeScreens_;
  ClickProfile activeProfile_;
  State state_ = State::Idle;
  QString status_ = "Idle";
  int remainingClicks_ = -1;
  int clicksExecuted_ = 0;
  int countdownRemaining_ = 0;
  bool running_ = false;
  bool ownsAutomation_ = false;
  quint64 runGeneration_ = 0;
};

