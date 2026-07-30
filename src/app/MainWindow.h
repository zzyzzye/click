#pragma once

#include <QMainWindow>

#include <memory>
#include <functional>
#include <optional>

#include "core/ClickController.h"
#include "core/MacroController.h"
#include "core/MacroRepository.h"
#include "core/SettingsRepository.h"
#include "platform/PlatformServices.h"

class ActionBar;
class ClickSettingsPage;
class HotkeySettingsPage;
class MacroRecordingPage;
class NavigationSidebar;
class PresetsAboutPage;
class QStackedWidget;
class StatusStrip;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  using MacroSafetyConfirmation = std::function<bool(QWidget*)>;
  using MacroNameProvider = std::function<QString(QWidget*)>;

  explicit MainWindow(QWidget* parent = nullptr);
  MainWindow(std::unique_ptr<ClickBackend> backend,
             std::unique_ptr<HotkeyService> hotkeyService,
             std::unique_ptr<SettingsRepository> settingsRepository,
             QWidget* parent = nullptr);
  MainWindow(std::unique_ptr<ClickBackend> backend,
             std::unique_ptr<HotkeyService> hotkeyService,
             std::unique_ptr<SettingsRepository> settingsRepository,
             MacroPlatformServices macroServices,
             std::unique_ptr<MacroRepository> macroRepository,
             MacroSafetyConfirmation safetyConfirmation,
             MacroNameProvider macroNameProvider,
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
  void handleMacroRecordRequested(const MacroRecordingOptions& options);
  void handleMacroPlayRequested(const QString& macroId,
                                const MacroPlaybackSettings& settings);
  void handleMacroStopRequested();
  void handleMacroRecordingCompleted(const MacroSequence& sequence);
  void handleMacroDeleteRequested(const QString& macroId);
  void handleMacroRenameRequested(const QString& macroId);
  void handleMacroWindowPointSelected(const QPoint& globalPoint);
  void handleMacroStateChanged(MacroControllerState state);

 private:
  void buildUi();
  void refreshPresetList(const QString& selectedName = {});
  void applyProfileToUi(const ClickProfile& profile);
  ClickProfile collectProfileFromUi() const;
  void updateRunningUi(bool running);
  void updatePermissionBanner();
  void applyWindowOnTop(bool enabled);
  bool validateHotkeys(const ClickProfile& profile, QString* errorMessage) const;
  QString selectedProfileName() const;
  void refreshMacroList(const QString& selectedId = {});
  void refreshMacroWindows(quintptr selectedNativeId = 0);
  std::optional<MacroSequence> findMacro(const QString& id) const;
  bool confirmMacroSafety();

  std::unique_ptr<ClickBackend> backend_;
  std::unique_ptr<HotkeyService> hotkeyService_;
  std::unique_ptr<SettingsRepository> settingsRepository_;
  AutomationCoordinator automationCoordinator_;
  ClickController controller_;
  MacroPlatformServices macroServices_;
  std::unique_ptr<MacroRepository> macroRepository_;
  MacroController macroController_;
  MacroSafetyConfirmation safetyConfirmation_;
  MacroNameProvider macroNameProvider_;
  QVector<MacroSequence> macros_;

  NavigationSidebar* sidebar_ = nullptr;
  StatusStrip* statusStrip_ = nullptr;
  QStackedWidget* pages_ = nullptr;
  ClickSettingsPage* clickPage_ = nullptr;
  HotkeySettingsPage* hotkeyPage_ = nullptr;
  MacroRecordingPage* macroPage_ = nullptr;
  PresetsAboutPage* presetsPage_ = nullptr;
  ActionBar* actionBar_ = nullptr;
  QString currentProfileName_ = "Default";
};
