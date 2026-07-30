#include "core/AutomationCoordinator.h"

namespace {

QString activityName(AutomationActivity activity) {
  switch (activity) {
    case AutomationActivity::Clicking: return "连点";
    case AutomationActivity::Recording: return "宏录制";
    case AutomationActivity::Playing: return "宏回放";
    case AutomationActivity::Idle: return "空闲";
  }
  return "自动化操作";
}

}  // namespace

AutomationCoordinator::AutomationCoordinator(QObject* parent) : QObject(parent) {}

bool AutomationCoordinator::tryAcquire(AutomationActivity activity, QString* error) {
  if (activity == AutomationActivity::Idle) {
    if (error) *error = "不能占用空闲状态。";
    return false;
  }
  if (activity_ != AutomationActivity::Idle) {
    if (error) {
      *error = QString("当前正在进行%1，不能同时启动其他功能。")
                   .arg(activityName(activity_));
    }
    return false;
  }
  activity_ = activity;
  emit activityChanged(activity_);
  return true;
}

void AutomationCoordinator::release(AutomationActivity activity) {
  if (activity_ != activity || activity == AutomationActivity::Idle) return;
  activity_ = AutomationActivity::Idle;
  emit activityChanged(activity_);
}

AutomationActivity AutomationCoordinator::activity() const {
  return activity_;
}

void AutomationCoordinator::requestEmergencyStop() {
  emit emergencyStopRequested();
}
