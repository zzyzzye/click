#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QGroupBox>
#include <memory>

#include "core/ClickController.h"
#include "core/SettingsRepository.h"
#include "platform/PlatformServices.h"

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
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
  void refreshPresetList(const QString& selectedName = QString());
  void applyProfileToUi(const ClickProfile& profile);
  ClickProfile collectProfileFromUi() const;
  void updateTargetModeUi();
  void updateRepeatModeUi();
  void updateRunningUi(bool running);
  void updatePermissionBanner();
  void applyWindowOnTop(bool enabled);
  bool validateHotkeys(QString* errorMessage) const;
  QString selectedProfileName() const;

  std::unique_ptr<ClickBackend> backend_;
  std::unique_ptr<HotkeyService> hotkeyService_;
  SettingsRepository settingsRepository_;
  ClickController controller_;

  QWidget* configPanel_ = nullptr;
  QGroupBox* clickGroup_ = nullptr;
  QGroupBox* hotkeyGroup_ = nullptr;
  QLabel* permissionLabel_ = nullptr;
  QLabel* quickStartLabel_ = nullptr;
  QPushButton* permissionButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLabel* countdownLabel_ = nullptr;
  QLabel* remainingLabel_ = nullptr;

  QSpinBox* intervalSpin_ = nullptr;
  QKeySequenceEdit* startStopHotkeyEdit_ = nullptr;
  QPushButton* startStopButton_ = nullptr;
};
