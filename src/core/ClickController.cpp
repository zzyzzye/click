#include "core/ClickController.h"

#include <algorithm>

#include "core/AutomationCoordinator.h"

ClickController::ClickController(ClickBackend* backend, QObject* parent)
    : ClickController(backend, static_cast<AutomationCoordinator*>(nullptr), parent) {}

ClickController::ClickController(ClickBackend* backend,
                                 AutomationCoordinator* coordinator,
                                 QObject* parent)
    : QObject(parent), backend_(backend), coordinator_(coordinator) {
  clickTimer_.setTimerType(Qt::PreciseTimer);
  countdownTimer_.setInterval(1000);

  connect(&clickTimer_, &QTimer::timeout, this, &ClickController::handleClickTick);
  connect(&countdownTimer_, &QTimer::timeout, this, &ClickController::handleCountdownTick);
}

void ClickController::start(const ClickProfile& profile) {
  if (running_) {
    return;
  }

  if (!backend_ || !backend_->hasAccessibilityPermission()) {
    if (backend_) {
      backend_->requestAccessibilityPermission();
    }
    state_ = State::PermissionDenied;
    setStatus("需要授予辅助功能权限后才能开始点击。");
    emit startRejected(status_);
    return;
  }

  QString ownershipError;
  if (coordinator_ &&
      !coordinator_->tryAcquire(AutomationActivity::Clicking, &ownershipError)) {
    setStatus(ownershipError);
    emit startRejected(ownershipError);
    return;
  }
  ownsAutomation_ = coordinator_ != nullptr;

  activeProfile_ = profile;
  activeProfile_.intervalMs = std::max(1, activeProfile_.intervalMs);
  activeProfile_.repeatCount = std::max(1, activeProfile_.repeatCount);
  activeProfile_.jitterRadius = std::max(0, activeProfile_.jitterRadius);
  activeProfile_.countdownSeconds = std::max(0, activeProfile_.countdownSeconds);

  running_ = true;
  remainingClicks_ =
      activeProfile_.repeatMode == RepeatMode::Finite ? activeProfile_.repeatCount : -1;
  emit remainingClicksChanged(remainingClicks_);
  emit runningChanged(true);

  if (activeProfile_.countdownSeconds > 0) {
    state_ = State::Countdown;
    countdownRemaining_ = activeProfile_.countdownSeconds;
    emit countdownChanged(countdownRemaining_);
    setStatus(QString("将在 %1 秒后开始").arg(countdownRemaining_));
    countdownTimer_.start();
    return;
  }

  beginRun();
}

void ClickController::stop() {
  if (!running_ && state_ != State::Completed) {
    state_ = State::Idle;
    setStatus("已停止");
    return;
  }

  finishRun(State::Idle, "已停止");
}

void ClickController::emergencyStop() {
  finishRun(State::Idle, "已紧急停止");
}

bool ClickController::isRunning() const {
  return running_;
}

int ClickController::remainingClicks() const {
  return remainingClicks_;
}

QString ClickController::currentStatus() const {
  return status_;
}

void ClickController::handleCountdownTick() {
  if (!running_) {
    return;
  }

  --countdownRemaining_;
  emit countdownChanged(std::max(0, countdownRemaining_));

  if (countdownRemaining_ <= 0) {
    countdownTimer_.stop();
    beginRun();
    return;
  }

  setStatus(QString("将在 %1 秒后开始").arg(countdownRemaining_));
}

void ClickController::handleClickTick() {
  performClick();
}

void ClickController::beginRun() {
  if (!running_) {
    return;
  }

  state_ = State::Running;
  setStatus("运行中");
  performClick();

  if (running_) {
    clickTimer_.start(activeProfile_.intervalMs);
  }
}

void ClickController::performClick() {
  if (!running_ || !backend_) {
    return;
  }

  const bool succeeded = activeProfile_.inputMode == InputMode::Keyboard
                             ? backend_->keyTap(activeProfile_)
                             : backend_->click(activeProfile_);
  if (!succeeded) {
    finishRun(State::Error, activeProfile_.inputMode == InputMode::Keyboard ? "按键执行失败" : "点击执行失败");
    return;
  }

  if (activeProfile_.repeatMode == RepeatMode::Finite) {
    remainingClicks_ = std::max(0, remainingClicks_ - 1);
    emit remainingClicksChanged(remainingClicks_);
    if (remainingClicks_ == 0) {
      finishRun(State::Completed, "已完成");
    }
  }
}

void ClickController::finishRun(State state, const QString& status) {
  clickTimer_.stop();
  countdownTimer_.stop();
  const bool wasRunning = running_;
  running_ = false;
  if (ownsAutomation_ && coordinator_) {
    coordinator_->release(AutomationActivity::Clicking);
    ownsAutomation_ = false;
  }
  countdownRemaining_ = 0;
  state_ = state;
  if (state_ != State::Completed && activeProfile_.repeatMode == RepeatMode::Infinite) {
    remainingClicks_ = -1;
  }
  setStatus(status);
  emit countdownChanged(0);
  if (wasRunning) {
    emit runningChanged(false);
  }
}

void ClickController::setStatus(const QString& status) {
  status_ = status;
  emit statusChanged(status_);
}
