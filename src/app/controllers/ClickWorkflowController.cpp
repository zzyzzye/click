#include "app/controllers/ClickWorkflowController.h"

#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>
#include <QWidget>

#include "app/pages/ClickSettingsPage.h"
#include "app/widgets/ActionBar.h"
#include "app/widgets/StatusStrip.h"
#include "core/AutomationCoordinator.h"
#include "core/ClickBackend.h"

ClickWorkflowController::ClickWorkflowController(
    std::unique_ptr<ClickBackend> backend,
    AutomationCoordinator* automationCoordinator,
    ClickSettingsPage* clickPage, ActionBar* actionBar,
    StatusStrip* statusStrip, ProfileProvider profileProvider,
    QWidget* dialogParent, QObject* parent)
    : QObject(parent),
      backend_(std::move(backend)),
      controller_(backend_.get(), automationCoordinator, this),
      clickPage_(clickPage),
      actionBar_(actionBar),
      statusStrip_(statusStrip),
      profileProvider_(std::move(profileProvider)),
      dialogParent_(dialogParent) {
  Q_ASSERT(backend_);
  Q_ASSERT(automationCoordinator);
  Q_ASSERT(clickPage_);
  Q_ASSERT(actionBar_);
  Q_ASSERT(statusStrip_);
  Q_ASSERT(profileProvider_);
  Q_ASSERT(dialogParent_);

  connect(actionBar_, &ActionBar::startStopRequested, this,
          &ClickWorkflowController::toggle);
  connect(clickPage_, &ClickSettingsPage::captureRequested, this,
          &ClickWorkflowController::capturePoint);
  connect(statusStrip_, &StatusStrip::permissionRequestRequested, this,
          &ClickWorkflowController::requestPermission);
  connect(&controller_, &ClickController::statusChanged, this,
          &ClickWorkflowController::handleStatusChanged);
  connect(&controller_, &ClickController::runningChanged, this,
          &ClickWorkflowController::handleRunningChanged);
  connect(&controller_, &ClickController::countdownChanged, this,
          &ClickWorkflowController::handleCountdownChanged);
  connect(&controller_, &ClickController::remainingClicksChanged, this,
          &ClickWorkflowController::handleRemainingClicksChanged);
  connect(&controller_, &ClickController::clicksExecutedChanged, this,
          &ClickWorkflowController::handleClicksExecutedChanged);
  connect(&controller_, &ClickController::startRejected, this,
          [this](const QString& reason) {
            QMessageBox::warning(dialogParent_, "无法启动", reason);
          });

  updatePermissionBanner();
}

ClickWorkflowController::~ClickWorkflowController() = default;

void ClickWorkflowController::toggle() {
  if (controller_.isRunning()) {
    controller_.stop();
    return;
  }
  const ClickProfile profile = profileProvider_();
  emit profilePersistRequested(profile);
  controller_.start(profile);
}

void ClickWorkflowController::capturePoint() {
  clickPage_->setFixedPoint(backend_->currentCursorPosition());
}

void ClickWorkflowController::emergencyStop() {
  controller_.emergencyStop();
}

void ClickWorkflowController::resetForProfile() {
  clicksExecuted_ = 0;
}

void ClickWorkflowController::requestPermission() {
  backend_->requestAccessibilityPermission();
#if defined(Q_OS_MACOS)
  QDesktopServices::openUrl(QUrl(
      "x-apple.systempreferences:com.apple.preference.security?"
      "Privacy_Accessibility"));
#endif
  updatePermissionBanner();
}

void ClickWorkflowController::handleStatusChanged(const QString& status) {
  statusStrip_->setStatus(status);
  if (status == "已完成") {
    statusStrip_->setProgress(
        QString("已完成 · 共执行 %1 次").arg(clicksExecuted_));
  } else if (status == "已停止" || status == "已紧急停止") {
    statusStrip_->setProgress(
        QString("%1 · 共执行 %2 次").arg(status).arg(clicksExecuted_));
  }
  updatePermissionBanner();
}

void ClickWorkflowController::handleRunningChanged(bool running) {
  actionBar_->setRunning(running);
  if (!running && controller_.currentStatus() == "运行中") {
    statusStrip_->setProgress(
        QString("已停止 · 共执行 %1 次").arg(clicksExecuted_));
  }
  if (running) {
    const ClickProfile profile = profileProvider_();
    const QString target =
        profile.inputMode == InputMode::Keyboard
            ? QString("键盘 %1").arg(profile.keyboardKey)
            : (profile.targetMode == TargetMode::FollowCursor
                   ? "跟随鼠标"
                   : QString("坐标 (%1, %2)")
                         .arg(profile.fixedPoint.x())
                         .arg(profile.fixedPoint.y()));
    actionBar_->setSummary(
        QString("运行中 · %1 · %2 毫秒 · %3")
            .arg(profile.inputMode == InputMode::Keyboard ? "按键" : "鼠标")
            .arg(profile.intervalMs)
            .arg(target));
  } else {
    actionBar_->setSummary(clickPage_->summary());
  }
  emit runningChanged(running);
}

void ClickWorkflowController::handleCountdownChanged(int seconds) {
  statusStrip_->setProgress(
      seconds > 0 ? QString("即将开始 · %1 秒").arg(seconds) : "就绪");
}

void ClickWorkflowController::handleRemainingClicksChanged(int remaining) {
  statusStrip_->setProgress(
      remaining < 0
          ? QString("已执行 %1 次 · 无限运行").arg(clicksExecuted_)
          : QString("已执行 %1 次 · 剩余 %2 次")
                .arg(clicksExecuted_)
                .arg(remaining));
}

void ClickWorkflowController::handleClicksExecutedChanged(int executed) {
  clicksExecuted_ = executed;
  if (controller_.isRunning()) {
    handleRemainingClicksChanged(controller_.remainingClicks());
  }
}

void ClickWorkflowController::updatePermissionBanner() {
  statusStrip_->setPermissionState(backend_->hasAccessibilityPermission());
}
