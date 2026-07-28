#pragma once

#include <QMainWindow>

#include <memory>

#include "core/ClickController.h"
#include "core/SettingsRepository.h"
#include "platform/PlatformServices.h"

class ActionBar;
class ClickSettingsPage;
class HotkeySettingsPage;
class NavigationSidebar;
class PresetsAboutPage;
class QStackedWidget;
class StatusStrip;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  MainWindow(std::unique_ptr<ClickBackend> backend,
             std::unique_ptr<HotkeyService> hotkeyService,
             std::unique_ptr<SettingsRepository> settingsRepository,
             QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void handleStartStop();
  void handleCapturePoint();
  void handleEmergencyStop();
  void handleSavePreset();
  void handleNewPreset();
  void handleRenamePreset();
  void handleDeletePreset();
  void handleLoadPreset();
  void handleProfileSelectionChanged();
  void handlePermissionRequest();
  void handleStatusChanged(const QString& status);
  void handleRunningChanged(bool running);
  void handleCountdownChanged(int seconds);
  void handleRemainingClicksChanged(int remaining);

 private:
  void buildUi();
  void refreshPresetList(const QString& selectedName = {});
  void applyProfileToUi(const ClickProfile& profile);
  ClickProfile collectProfileFromUi() const;
  void updateRunningUi(bool running);
  void updatePermissionBanner();
  void applyWindowOnTop(bool enabled);
  bool validateHotkeys(QString* errorMessage) const;
  QString selectedProfileName() const;

  std::unique_ptr<ClickBackend> backend_;
  std::unique_ptr<HotkeyService> hotkeyService_;
  std::unique_ptr<SettingsRepository> settingsRepository_;
  ClickController controller_;

  NavigationSidebar* sidebar_ = nullptr;
  StatusStrip* statusStrip_ = nullptr;
  QStackedWidget* pages_ = nullptr;
  ClickSettingsPage* clickPage_ = nullptr;
  HotkeySettingsPage* hotkeyPage_ = nullptr;
  PresetsAboutPage* presetsPage_ = nullptr;
  ActionBar* actionBar_ = nullptr;
  QString currentProfileName_ = "Default";
};
