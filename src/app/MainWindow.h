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
  std::unique_ptr<SettingsRepository> settingsRepository_;
  ClickController controller_;

  QWidget* configPanel_ = nullptr;
  QGroupBox* profileGroup_ = nullptr;
  QGroupBox* clickGroup_ = nullptr;
  QGroupBox* hotkeyGroup_ = nullptr;
  QLabel* permissionLabel_ = nullptr;
  QLabel* quickStartLabel_ = nullptr;
  QPushButton* permissionButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLabel* countdownLabel_ = nullptr;
  QLabel* remainingLabel_ = nullptr;

  QListWidget* profileList_ = nullptr;
  QPushButton* newPresetButton_ = nullptr;
  QPushButton* savePresetButton_ = nullptr;
  QPushButton* renamePresetButton_ = nullptr;
  QPushButton* deletePresetButton_ = nullptr;
  QPushButton* loadPresetButton_ = nullptr;

  QSpinBox* intervalSpin_ = nullptr;
  QComboBox* buttonCombo_ = nullptr;
  QComboBox* targetModeCombo_ = nullptr;
  QSpinBox* fixedXSpin_ = nullptr;
  QSpinBox* fixedYSpin_ = nullptr;
  QPushButton* capturePointButton_ = nullptr;
  QSpinBox* jitterSpin_ = nullptr;
  QComboBox* repeatModeCombo_ = nullptr;
  QSpinBox* repeatCountSpin_ = nullptr;
  QSpinBox* countdownSpin_ = nullptr;
  QCheckBox* alwaysOnTopCheck_ = nullptr;

  QKeySequenceEdit* startStopHotkeyEdit_ = nullptr;
  QKeySequenceEdit* captureHotkeyEdit_ = nullptr;
  QKeySequenceEdit* emergencyHotkeyEdit_ = nullptr;
  QPushButton* startStopButton_ = nullptr;

  QString currentProfileName_ = "Default";
};
