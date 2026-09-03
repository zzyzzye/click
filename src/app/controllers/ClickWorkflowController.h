#pragma once

#include <QObject>

#include <functional>
#include <memory>

#include "core/ClickController.h"
#include "core/ClickTypes.h"

class ActionBar;
class AutomationCoordinator;
class ClickBackend;
class ClickSettingsPage;
class StatusStrip;
class QWidget;

// 连点工作流：管理后端、运行状态、权限和状态栏反馈。
class ClickWorkflowController final : public QObject {
  Q_OBJECT

 public:
  using ProfileProvider = std::function<ClickProfile()>;

  ClickWorkflowController(std::unique_ptr<ClickBackend> backend,
                          AutomationCoordinator* automationCoordinator,
                          ClickSettingsPage* clickPage, ActionBar* actionBar,
                          StatusStrip* statusStrip,
                          ProfileProvider profileProvider,
                          QWidget* dialogParent,
                          QObject* parent = nullptr);
  ~ClickWorkflowController() override;

  bool isRunning() const { return controller_.isRunning(); }
  void toggle();
  void capturePoint();
  void emergencyStop();
  void resetForProfile();

 signals:
  void profilePersistRequested(const ClickProfile& profile);
  void runningChanged(bool running);

 private:
  void requestPermission();
  void handleStatusChanged(const QString& status);
  void handleRunningChanged(bool running);
  void handleCountdownChanged(int seconds);
  void handleRemainingClicksChanged(int remaining);
  void handleClicksExecutedChanged(int executed);
  void updatePermissionBanner();

  std::unique_ptr<ClickBackend> backend_;
  ClickController controller_;
  ClickSettingsPage* clickPage_ = nullptr;
  ActionBar* actionBar_ = nullptr;
  StatusStrip* statusStrip_ = nullptr;
  ProfileProvider profileProvider_;
  QWidget* dialogParent_ = nullptr;
  int clicksExecuted_ = 0;
};
