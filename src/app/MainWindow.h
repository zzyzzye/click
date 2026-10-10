#pragma once

#include <QMainWindow>

#include <functional>
#include <memory>

#include "core/AutomationCoordinator.h"
#include "core/ClickController.h"
#include "core/MacroController.h"
#include "core/MacroRepository.h"
#include "core/SettingsRepository.h"
#include "platform/PlatformServices.h"
#include "platform/WindowStyleService.h"

class ActionBar;
class CaptionBar;
class ClickBackend;
class ClickSettingsPage;
class ClickWorkflowController;
class HotkeyController;
class HotkeyService;
class HotkeySettingsPage;
class MacroRecordingPage;
class MacroRepository;
class MacroWorkflowController;
class NavigationSidebar;
class PresetsAboutPage;
class ProfileController;
class QStackedWidget;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class SettingsRepository;
class StatusStrip;
class UpdateController;
class WindowChromeController;
class WindowStyleService;
class WorkflowUiStateController;
struct MacroPlatformServices;

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
             std::unique_ptr<WindowStyleService> windowStyle,
             QWidget* parent = nullptr);
  ~MainWindow() override;

 private:
  void buildUi();
  void installInputFilters();
  void connectControllers();
  void showEvent(QShowEvent* event) override;
  bool nativeEvent(const QByteArray& eventType, void* message,
                   qintptr* result) override;
  void changeEvent(QEvent* event) override;

  AutomationCoordinator automationCoordinator_;
  std::unique_ptr<WindowStyleService> windowStyle_;
  std::unique_ptr<WindowChromeController> windowChrome_;
  std::unique_ptr<ProfileController> profileController_;
  std::unique_ptr<HotkeyController> hotkeyController_;
  std::unique_ptr<ClickWorkflowController> clickController_;
  std::unique_ptr<MacroWorkflowController> macroController_;
  std::unique_ptr<WorkflowUiStateController> workflowUiController_;
  std::unique_ptr<UpdateController> updateController_;

  NavigationSidebar* sidebar_ = nullptr;
  CaptionBar* captionBar_ = nullptr;
  StatusStrip* statusStrip_ = nullptr;
  QStackedWidget* pages_ = nullptr;
  QGraphicsOpacityEffect* pageOpacity_ = nullptr;
  QPropertyAnimation* pageTransition_ = nullptr;
  bool reducedMotion_ = false;
  ClickSettingsPage* clickPage_ = nullptr;
  HotkeySettingsPage* hotkeyPage_ = nullptr;
  MacroRecordingPage* macroPage_ = nullptr;
  PresetsAboutPage* presetsPage_ = nullptr;
  ActionBar* actionBar_ = nullptr;
};
