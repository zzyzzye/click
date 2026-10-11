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
    setStatus("输入控制不可用，请检查系统权限；Linux 需要 X11 桌面。");
    emit startRejected(status_);
    return;
  }

  QString validationError;
  if (!validateInput(profile, &validationError, false)) {
    state_ = State::Error;
    setStatus(validationError);
    emit startRejected(validationError);
    return;
  }
  activeScreens_ =
      profile.inputMode == InputMode::Mouse && profile.targetMode == TargetMode::FixedPoint
          ? backend_->screenRects()
          : QVector<QRect>{};
  validationClock_.invalidate();

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
  const auto generation = ++runGeneration_;
  remainingClicks_ =
      activeProfile_.repeatMode == RepeatMode::Finite ? activeProfile_.repeatCount : -1;
  clicksExecuted_ = 0;
  emit remainingClicksChanged(remainingClicks_);
  if (!running_ || generation != runGeneration_) return;
  emit clicksExecutedChanged(clicksExecuted_);
  if (!running_ || generation != runGeneration_) return;
  emit runningChanged(true);
  if (!running_ || generation != runGeneration_) return;

  if (activeProfile_.countdownSeconds > 0) {
    state_ = State::Countdown;
    countdownRemaining_ = activeProfile_.countdownSeconds;
    emit countdownChanged(countdownRemaining_);
    if (!running_ || generation != runGeneration_) return;
    setStatus(QString("将在 %1 秒后开始").arg(countdownRemaining_));
    if (!running_ || generation != runGeneration_) return;
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

int ClickController::clicksExecuted() const {
  return clicksExecuted_;
}

QString ClickController::currentStatus() const {
  return status_;
}

void ClickController::handleCountdownTick() {
  if (!running_) {
    return;
  }

  --countdownRemaining_;
  const auto generation = runGeneration_;
  emit countdownChanged(std::max(0, countdownRemaining_));
  if (!running_ || generation != runGeneration_)
    return;

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
  // 倒计时结束后重新检查，不能沿用几秒前的权限与显示器状态。
  validationClock_.invalidate();

  state_ = State::Running;
  const auto generation = runGeneration_;
  setStatus("运行中");
  if (!running_ || generation != runGeneration_) return;
  performClick();

  if (running_ && generation == runGeneration_) {
    clickTimer_.start(activeProfile_.intervalMs);
  }
}

void ClickController::performClick() {
  if (!running_ || !backend_) {
    return;
  }

  const auto generation = runGeneration_;
  if (!validationClock_.isValid() || validationClock_.elapsed() >= 250) {
    QString error;
    if (!validateInput(activeProfile_, &error, true)) {
      finishRun(State::Error, error);
      return;
    }
    validationClock_.restart();
  }
  const bool succeeded = activeProfile_.inputMode == InputMode::Keyboard
                             ? backend_->keyTap(activeProfile_)
                             : backend_->click(activeProfile_);
  if (!succeeded) {
    finishRun(State::Error, activeProfile_.inputMode == InputMode::Keyboard ? "按键执行失败" : "点击执行失败");
    return;
  }

  ++clicksExecuted_;
  emit clicksExecutedChanged(clicksExecuted_);
  // 进度订阅者可能已请求停止，不能再将这次停止改写为正常完成。
  if (!running_ || generation != runGeneration_) return;

  if (activeProfile_.repeatMode == RepeatMode::Finite) {
    remainingClicks_ = std::max(0, remainingClicks_ - 1);
    emit remainingClicksChanged(remainingClicks_);
    if (!running_ || generation != runGeneration_) return;
    if (remainingClicks_ == 0) {
      finishRun(State::Completed, "已完成");
    }
  }
}

bool ClickController::validateInput(const ClickProfile& profile, QString* error,
                                    bool checkLayout) const {
  if (!backend_->hasAccessibilityPermission()) {
    *error = "输入控制权限已失效，已停止；请检查系统权限后重新开始。";
    return false;
  }
  if (profile.inputMode == InputMode::Keyboard && !backend_->supportsKey(profile.keyboardKey)) {
    *error = "当前平台不支持所选按键或组合键，请更换按键后重新开始。";
    return false;
  }
  if (profile.inputMode != InputMode::Mouse || profile.targetMode != TargetMode::FixedPoint)
    return true;
  const auto screens = backend_->screenRects();
  if (screens.isEmpty()) {
    *error = "无法读取显示器布局，请检查系统输入支持后重试。";
    return false;
  }
  if (checkLayout && screens != activeScreens_) {
    *error = "显示器布局已变化，已停止；请重新捕获固定坐标后开始。";
    return false;
  }
  if (std::none_of(screens.cbegin(), screens.cend(),
                   [&](const QRect& r) { return r.contains(profile.fixedPoint); })) {
    *error = "固定坐标不在当前屏幕内，请重新捕获位置后开始。";
    return false;
  }
  return true;
}

void ClickController::finishRun(State state, const QString& status) {
  clickTimer_.stop();
  countdownTimer_.stop();
  const bool wasRunning = running_;
  running_ = false;
  ++runGeneration_;
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
