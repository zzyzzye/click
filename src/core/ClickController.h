#pragma once

#include <QObject>
#include <QTimer>

#include "core/ClickBackend.h"

class ClickController : public QObject {
  Q_OBJECT

 public:
  explicit ClickController(ClickBackend* backend, QObject* parent = nullptr);

  void start(const ClickProfile& profile);
  void stop();
  void emergencyStop();

  bool isRunning() const;
  int remainingClicks() const;
  QString currentStatus() const;

 signals:
  void statusChanged(const QString& status);
  void runningChanged(bool running);
  void remainingClicksChanged(int remaining);
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
  void finishRun(State state, const QString& status);
  void setStatus(const QString& status);

  ClickBackend* backend_;
  QTimer clickTimer_;
  QTimer countdownTimer_;
  ClickProfile activeProfile_;
  State state_ = State::Idle;
  QString status_ = "Idle";
  int remainingClicks_ = -1;
  int countdownRemaining_ = 0;
  bool running_ = false;
};

