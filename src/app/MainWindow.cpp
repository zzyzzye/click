#include "app/MainWindow.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <utility>

#include "app/UiStyle.h"
#include "app/controllers/ClickWorkflowController.h"
#include "app/controllers/HotkeyController.h"
#include "app/controllers/MacroWorkflowController.h"
#include "app/controllers/ProfileController.h"
#include "app/controllers/UpdateController.h"
#include "app/controllers/WindowChromeController.h"
#include "app/controllers/WorkflowUiStateController.h"
#include "app/pages/ClickSettingsPage.h"
#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/MacroRecordingPage.h"
#include "app/pages/PresetsAboutPage.h"
#include "app/widgets/ActionBar.h"
#include "app/widgets/CaptionBar.h"
#include "app/widgets/NavigationSidebar.h"
#include "app/widgets/SmoothScrollArea.h"
#include "app/widgets/StatusStrip.h"
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"
#include "core/MacroRepository.h"
#include "core/SettingsRepository.h"
#include "platform/PlatformServices.h"
#include "platform/WindowStyleService.h"

namespace {

class IgnoreWheelChangeFilter final : public QObject {
 public:
  using QObject::QObject;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() != QEvent::Wheel) {
      return QObject::eventFilter(watched, event);
    }
    if (auto* combo = qobject_cast<QComboBox*>(watched)) {
      auto* scroll = combo->parentWidget();
      while (scroll && !dynamic_cast<SmoothScrollArea*>(scroll)) {
        scroll = scroll->parentWidget();
      }
      if (auto* area = dynamic_cast<SmoothScrollArea*>(scroll)) {
        area->scrollForWheelEvent(*static_cast<QWheelEvent*>(event));
      }
      return true;
    }
    return qobject_cast<QAbstractSpinBox*>(watched) != nullptr;
  }
};

bool defaultMacroSafetyConfirmation(QWidget* parent) {
  return QMessageBox::question(
             parent, "开始键鼠录制",
             "录制文件会保存按键和操作时间，可能反映敏感输入。\n\n"
             "请不要在录制过程中输入密码、验证码或其他机密信息。是否继续？",
             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) ==
         QMessageBox::Yes;
}

QString defaultMacroName(QWidget* parent) {
  bool accepted = false;
  const QString suggested =
      QString("录制 %1")
          .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss"));
  const QString name =
      QInputDialog::getText(parent, "保存录制", "宏名称", QLineEdit::Normal,
                            suggested, &accepted)
          .trimmed();
  return accepted ? name : QString();
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : MainWindow(createClickBackend(), createHotkeyService(),
                 std::make_unique<SettingsRepository>(),
                 createMacroPlatformServices(),
                 std::make_unique<MacroRepository>(), {}, {},
                 createWindowStyleService(), parent) {}

MainWindow::MainWindow(
    std::unique_ptr<ClickBackend> backend,
    std::unique_ptr<HotkeyService> hotkeyService,
    std::unique_ptr<SettingsRepository> settingsRepository, QWidget* parent)
    : MainWindow(std::move(backend), std::move(hotkeyService),
                 std::move(settingsRepository), MacroPlatformServices{},
                 nullptr, {}, {}, {}, parent) {}

MainWindow::MainWindow(
    std::unique_ptr<ClickBackend> backend,
    std::unique_ptr<HotkeyService> hotkeyService,
    std::unique_ptr<SettingsRepository> settingsRepository,
    MacroPlatformServices macroServices,
    std::unique_ptr<MacroRepository> macroRepository,
    MacroSafetyConfirmation safetyConfirmation,
    MacroNameProvider macroNameProvider,
    std::unique_ptr<WindowStyleService> windowStyle, QWidget* parent)
    : QMainWindow(parent),
      automationCoordinator_(this),
      windowStyle_(std::move(windowStyle)) {
  if (!safetyConfirmation) safetyConfirmation = defaultMacroSafetyConfirmation;
  if (!macroNameProvider) macroNameProvider = defaultMacroName;
  if (!windowStyle_) windowStyle_ = createWindowStyleService();

  buildUi();
  windowChrome_ = std::make_unique<WindowChromeController>(
      this, captionBar_, windowStyle_.get());
  profileController_ = std::make_unique<ProfileController>(
      std::move(settingsRepository), clickPage_, hotkeyPage_, presetsPage_,
      actionBar_, windowChrome_.get(), this, this);
  hotkeyController_ = std::make_unique<HotkeyController>(
      std::move(hotkeyService), hotkeyPage_, macroPage_, actionBar_,
      [this] { return profileController_->currentProfile(); }, this);
  clickController_ = std::make_unique<ClickWorkflowController>(
      std::move(backend), &automationCoordinator_, clickPage_, actionBar_,
      statusStrip_, [this] { return profileController_->currentProfile(); },
      this, this);
  macroController_ = std::make_unique<MacroWorkflowController>(
      std::move(macroServices), std::move(macroRepository),
      &automationCoordinator_, macroPage_, statusStrip_,
      [this] { return profileController_->currentProfile(); },
      std::move(safetyConfirmation), std::move(macroNameProvider),
      [this] { return profileController_->macroSafetyAcknowledged(); },
      [this] { profileController_->acknowledgeMacroSafety(); },
      [this] { return static_cast<quintptr>(winId()); }, this, this);
  workflowUiController_ = std::make_unique<WorkflowUiStateController>(
      clickPage_, hotkeyPage_, macroPage_, presetsPage_, actionBar_,
      macroController_->isSupported(), this);
  updateController_ = std::make_unique<UpdateController>(
      [this] {
        return !clickController_->isRunning() &&
               macroController_->state() == MacroControllerState::Idle;
      },
      this);

  installInputFilters();
  connectControllers();
  macroController_->initialize();
  profileController_->initialize();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
  setWindowTitle("ClickFlow");

  auto* central = new QWidget(this);
  setCentralWidget(central);
  auto* outer = new QVBoxLayout(central);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  captionBar_ = new CaptionBar(windowTitle(), central);
  outer->addWidget(captionBar_);

  auto* shellContainer = new QWidget(central);
  auto* shell = new QHBoxLayout(shellContainer);
  shell->setContentsMargins(0, 0, 0, 0);
  shell->setSpacing(0);
  sidebar_ = new NavigationSidebar(shellContainer);
  shell->addWidget(sidebar_);

  auto* content = new QWidget(shellContainer);
  content->setObjectName("contentSurface");
  auto* layout = new QVBoxLayout(content);
  layout->setContentsMargins(20, 18, 20, 18);
  layout->setSpacing(14);
  statusStrip_ = new StatusStrip(content);
  pages_ = new QStackedWidget(content);
  pages_->setObjectName("contentPages");
  clickPage_ = new ClickSettingsPage(pages_);
  macroPage_ = new MacroRecordingPage(pages_);
  hotkeyPage_ = new HotkeySettingsPage(pages_);
  presetsPage_ = new PresetsAboutPage(pages_);

  const auto addScrollablePage = [this](QWidget* page) {
    auto* scroll = new SmoothScrollArea(pages_);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFrameShape(QFrame::NoFrame);
    page->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    scroll->setWidget(page);
    pages_->addWidget(scroll);
  };
  addScrollablePage(clickPage_);
  addScrollablePage(macroPage_);
  addScrollablePage(hotkeyPage_);
  addScrollablePage(presetsPage_);

  actionBar_ = new ActionBar(content);
  layout->addWidget(statusStrip_);
  layout->addWidget(pages_, 1);
  layout->addWidget(actionBar_);
  shell->addWidget(content, 1);
  outer->addWidget(shellContainer, 1);

  setMinimumSize(820, 560);
  resize(920, 620);
  setStyleSheet(clickFlowStyleSheet(windowStyle_->usesBackdrop()));
  windowStyle_->prepare(this);
}

void MainWindow::installInputFilters() {
  auto* filter = new IgnoreWheelChangeFilter(this);
  for (auto* combo : findChildren<QComboBox*>()) {
    combo->installEventFilter(filter);
  }
  for (auto* spinBox : findChildren<QAbstractSpinBox*>()) {
    spinBox->installEventFilter(filter);
  }
}

void MainWindow::connectControllers() {
  connect(sidebar_, &NavigationSidebar::pageSelected, this,
          [this](ShellPage page) {
            pages_->setCurrentIndex(static_cast<int>(page));
            actionBar_->setVisible(page != ShellPage::MacroRecording);
          });

  connect(profileController_.get(), &ProfileController::profileApplied,
          hotkeyController_.get(), &HotkeyController::applyProfile);
  connect(profileController_.get(), &ProfileController::profileApplied,
          clickController_.get(),
          [this](const ClickProfile&) { clickController_->resetForProfile(); });
  connect(profileController_.get(), &ProfileController::statusChanged,
          statusStrip_, &StatusStrip::setStatus);

  connect(hotkeyController_.get(),
          &HotkeyController::profilePersistRequested,
          profileController_.get(), &ProfileController::saveLastUsed);
  connect(hotkeyController_.get(), &HotkeyController::clickToggleRequested,
          clickController_.get(), &ClickWorkflowController::toggle);
  connect(hotkeyController_.get(), &HotkeyController::capturePointRequested,
          clickController_.get(), &ClickWorkflowController::capturePoint);
  connect(hotkeyController_.get(), &HotkeyController::emergencyStopRequested,
          clickController_.get(), &ClickWorkflowController::emergencyStop);
  connect(hotkeyController_.get(), &HotkeyController::emergencyStopRequested,
          macroController_.get(), &MacroWorkflowController::emergencyStop);
  connect(hotkeyController_.get(),
          &HotkeyController::macroRecordToggleRequested,
          macroController_.get(), &MacroWorkflowController::toggleRecording);
  connect(hotkeyController_.get(),
          &HotkeyController::macroPlaybackToggleRequested,
          macroController_.get(), &MacroWorkflowController::togglePlayback);

  connect(clickController_.get(),
          &ClickWorkflowController::profilePersistRequested,
          profileController_.get(), &ProfileController::saveLastUsed);
  connect(clickController_.get(), &ClickWorkflowController::runningChanged,
          workflowUiController_.get(),
          &WorkflowUiStateController::setClickRunning);
  connect(macroController_.get(), &MacroWorkflowController::stateChanged,
          workflowUiController_.get(),
          &WorkflowUiStateController::setMacroState);

  connect(presetsPage_, &PresetsAboutPage::updateRequested,
          updateController_.get(), &UpdateController::handleAction);
  connect(updateController_.get(), &UpdateController::statusChanged,
          presetsPage_, &PresetsAboutPage::setUpdateStatus);
  connect(updateController_.get(), &UpdateController::actionChanged,
          presetsPage_, &PresetsAboutPage::setUpdateAction);
  connect(updateController_.get(), &UpdateController::quitRequested, qApp,
          &QCoreApplication::quit);
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  windowChrome_->handleShown();
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message,
                             qintptr* result) {
  if (windowChrome_->handleNativeEvent(eventType, message, result)) return true;
  return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
  windowChrome_->handleWindowStateChange(event);
}
