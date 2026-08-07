#include "app/MainWindow.h"

#include <QDesktopServices>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <utility>
#include <algorithm>

#include "app/pages/ClickSettingsPage.h"
#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/MacroRecordingPage.h"
#include "app/pages/PresetsAboutPage.h"
#include "app/UiStyle.h"
#include "app/widgets/ActionBar.h"
#include "app/widgets/NavigationSidebar.h"
#include "app/widgets/SmoothScrollArea.h"
#include "app/widgets/StatusStrip.h"
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"

namespace {

class IgnoreWheelChangeFilter final : public QObject {
 public:
  using QObject::QObject;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() != QEvent::Wheel) return QObject::eventFilter(watched, event);

    if (auto* combo = qobject_cast<QComboBox*>(watched)) {
      auto* scroll = combo->parentWidget();
      while (scroll && !dynamic_cast<SmoothScrollArea*>(scroll)) scroll = scroll->parentWidget();
      if (auto* const area = dynamic_cast<SmoothScrollArea*>(scroll)) {
        area->scrollForWheelEvent(*static_cast<QWheelEvent*>(event));
      }
      return true;
    }

    if (qobject_cast<QAbstractSpinBox*>(watched)) return true;
    return QObject::eventFilter(watched, event);
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
      QString("录制 %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss"));
  const QString name = QInputDialog::getText(parent, "保存录制", "宏名称",
                                              QLineEdit::Normal, suggested,
                                              &accepted).trimmed();
  return accepted ? name : QString();
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : MainWindow(createClickBackend(), createHotkeyService(),
                 std::make_unique<SettingsRepository>(),
                 createMacroPlatformServices(),
                 std::make_unique<MacroRepository>(), {}, {}, parent) {}

MainWindow::MainWindow(std::unique_ptr<ClickBackend> backend,
                       std::unique_ptr<HotkeyService> hotkeyService,
                       std::unique_ptr<SettingsRepository> settingsRepository,
                       QWidget* parent)
    : MainWindow(std::move(backend), std::move(hotkeyService),
                 std::move(settingsRepository), MacroPlatformServices{}, nullptr,
                 {}, {}, parent) {}

MainWindow::MainWindow(
    std::unique_ptr<ClickBackend> backend,
    std::unique_ptr<HotkeyService> hotkeyService,
    std::unique_ptr<SettingsRepository> settingsRepository,
    MacroPlatformServices macroServices,
    std::unique_ptr<MacroRepository> macroRepository,
    MacroSafetyConfirmation safetyConfirmation,
    MacroNameProvider macroNameProvider, QWidget* parent)
    : QMainWindow(parent),
      backend_(std::move(backend)),
      hotkeyService_(std::move(hotkeyService)),
      settingsRepository_(std::move(settingsRepository)),
      automationCoordinator_(this),
      controller_(backend_.get(), &automationCoordinator_, this),
      macroServices_(std::move(macroServices)),
      macroRepository_(std::move(macroRepository)),
      macroController_(macroServices_.recorder.get(), macroServices_.player.get(),
                       &automationCoordinator_, this),
      safetyConfirmation_(std::move(safetyConfirmation)),
      macroNameProvider_(std::move(macroNameProvider)) {
  if (!safetyConfirmation_) safetyConfirmation_ = defaultMacroSafetyConfirmation;
  if (!macroNameProvider_) macroNameProvider_ = defaultMacroName;
  buildUi();

  auto* ignoreWheelFilter = new IgnoreWheelChangeFilter(this);
  for (auto* combo : findChildren<QComboBox*>()) {
    combo->installEventFilter(ignoreWheelFilter);
  }
  for (auto* spinBox : findChildren<QAbstractSpinBox*>()) {
    spinBox->installEventFilter(ignoreWheelFilter);
  }

  connect(actionBar_, &ActionBar::startStopRequested, this, &MainWindow::handleStartStop);
  connect(clickPage_, &ClickSettingsPage::captureRequested, this, &MainWindow::handleCapturePoint);
  connect(clickPage_, &ClickSettingsPage::alwaysOnTopChanged, this, &MainWindow::applyWindowOnTop);
  connect(clickPage_, &ClickSettingsPage::settingsChanged, this,
          [this] { actionBar_->setSummary(clickPage_->summary()); });
  connect(sidebar_, &NavigationSidebar::pageSelected, this,
          [this](ShellPage page) {
            pages_->setCurrentIndex(static_cast<int>(page));
            actionBar_->setVisible(page != ShellPage::MacroRecording);
          });

  connect(macroPage_, &MacroRecordingPage::recordRequested, this,
          &MainWindow::handleMacroRecordRequested);
  connect(macroPage_, &MacroRecordingPage::playRequested, this,
          &MainWindow::handleMacroPlayRequested);
  connect(macroPage_, &MacroRecordingPage::stopRequested, this,
          &MainWindow::handleMacroStopRequested);
  connect(macroPage_, &MacroRecordingPage::deleteRequested, this,
          &MainWindow::handleMacroDeleteRequested);
  connect(macroPage_, &MacroRecordingPage::renameRequested, this,
          &MainWindow::handleMacroRenameRequested);
  connect(macroPage_, &MacroRecordingPage::refreshWindowsRequested, this,
          [this] { refreshMacroWindows(); });
  connect(macroPage_, &MacroRecordingPage::windowPointSelected, this,
          &MainWindow::handleMacroWindowPointSelected);
  connect(hotkeyPage_, &HotkeySettingsPage::hotkeysChanged, this, [this] {
    const ClickProfile profile = collectProfileFromUi();
    QString error;
    const bool valid = validateHotkeys(profile, &error);
    if (!valid) {
      macroPage_->setError(error);
      if (globalHotkeysEnabled_) {
        disableGlobalHotkeys(QString("热键无效：%1").arg(error));
      }
      return;
    }

    settingsRepository_->saveLastUsedProfile(profile);
    if (globalHotkeysEnabled_) {
      tryEnableGlobalHotkeys(profile);
    } else {
      macroPage_->setHotkeys(profile.hotkeys, false);
      hotkeyPage_->setActivationState(
          false, "当前未占用任何系统热键");
    }
  });
  connect(hotkeyPage_, &HotkeySettingsPage::activationRequested, this,
          &MainWindow::handleHotkeyActivationRequested);

  connect(presetsPage_, &PresetsAboutPage::newRequested, this, &MainWindow::handleNewPreset);
  connect(presetsPage_, &PresetsAboutPage::saveRequested, this, &MainWindow::handleSavePreset);
  connect(presetsPage_, &PresetsAboutPage::renameRequested, this, &MainWindow::handleRenamePreset);
  connect(presetsPage_, &PresetsAboutPage::deleteRequested, this, &MainWindow::handleDeletePreset);
  connect(presetsPage_, &PresetsAboutPage::loadRequested, this, &MainWindow::handleLoadPreset);
  connect(presetsPage_, &PresetsAboutPage::selectionChanged, this,
          &MainWindow::handleProfileSelectionChanged);

  connect(&controller_, &ClickController::statusChanged, this, &MainWindow::handleStatusChanged);
  connect(&controller_, &ClickController::runningChanged, this, &MainWindow::handleRunningChanged);
  connect(&controller_, &ClickController::countdownChanged, this, &MainWindow::handleCountdownChanged);
  connect(&controller_, &ClickController::remainingClicksChanged, this,
          &MainWindow::handleRemainingClicksChanged);
  connect(&controller_, &ClickController::startRejected, this,
          [this](const QString& reason) { QMessageBox::warning(this, "无法启动", reason); });

  connect(&macroController_, &MacroController::stateChanged, this,
          &MainWindow::handleMacroStateChanged);
  connect(&macroController_, &MacroController::recordingProgress, macroPage_,
          &MacroRecordingPage::setRecordingProgress);
  connect(&macroController_, &MacroController::playbackProgress, macroPage_,
          &MacroRecordingPage::setPlaybackProgress);
  connect(&macroController_, &MacroController::recordingCompleted, this,
          &MainWindow::handleMacroRecordingCompleted);
  connect(&macroController_, &MacroController::statusChanged, this,
          [this](const QString& status) { statusStrip_->setStatus(status); });
  connect(&macroController_, &MacroController::failed, this,
          [this](const QString& reason) {
            macroPage_->setError(reason);
            statusStrip_->setStatus(reason);
          });

  connect(hotkeyService_.get(), &HotkeyService::startStopPressed, this, &MainWindow::handleStartStop);
  connect(hotkeyService_.get(), &HotkeyService::capturePointPressed, this, &MainWindow::handleCapturePoint);
  connect(hotkeyService_.get(), &HotkeyService::emergencyStopPressed, this, &MainWindow::handleEmergencyStop);
  connect(hotkeyService_.get(), &HotkeyService::macroRecordPressed, this,
          [this] {
            if (macroController_.isRecording()) handleMacroStopRequested();
            else handleMacroRecordRequested(macroPage_->recordingOptions());
          });
  connect(hotkeyService_.get(), &HotkeyService::macroPlaybackPressed, this,
          [this] {
            if (macroController_.isPlaying()) handleMacroStopRequested();
            else handleMacroPlayRequested(macroPage_->selectedMacroId(),
                                          macroPage_->playbackSettings());
          });
  connect(hotkeyService_.get(), &HotkeyService::registrationFailed, this,
          [this](const QString& message) {
            lastHotkeyRegistrationError_ = message;
          });

  ClickProfile profile;
  if (const auto saved = settingsRepository_->loadLastUsedProfile(); saved.has_value())
    profile = *saved;
  applyProfileToUi(profile);
  refreshPresetList(profile.name);
  updatePermissionBanner();
  hotkeyPage_->setActivationState(false, "当前未占用任何系统热键");
  macroPage_->setHotkeys(profile.hotkeys, false);
  refreshMacroList();
  refreshMacroWindows();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
  auto* central = new QWidget(this);
  setCentralWidget(central);
  auto* shell = new QHBoxLayout(central);
  shell->setContentsMargins(0, 0, 0, 0);
  shell->setSpacing(0);
  sidebar_ = new NavigationSidebar(central);
  shell->addWidget(sidebar_);

  auto* content = new QWidget(central);
  content->setObjectName("contentSurface");
  auto* layout = new QVBoxLayout(content);
  layout->setContentsMargins(20, 18, 20, 18);
  layout->setSpacing(14);
  statusStrip_ = new StatusStrip(content);
  pages_ = new QStackedWidget(content);
  pages_->setObjectName("contentPages");
  clickPage_ = new ClickSettingsPage(pages_);
  macroPage_ = new MacroRecordingPage(pages_);
  const bool macroSupported = macroServices_.windowService && macroServices_.recorder &&
                              macroServices_.player && macroRepository_;
  macroPage_->setSupported(
      macroSupported,
      macroSupported ? QString() : "键鼠录制当前仅支持 Windows 10/11。");
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

  setWindowTitle("ClickFlow");
  setMinimumSize(820, 560);
  resize(920, 620);
  setStyleSheet(clickFlowStyleSheet());
}

void MainWindow::handleStartStop() {
  if (controller_.isRunning()) { controller_.stop(); return; }
  ClickProfile profile = collectProfileFromUi();
  settingsRepository_->saveLastUsedProfile(profile);
  controller_.start(profile);
}
void MainWindow::handleCapturePoint() { clickPage_->setFixedPoint(backend_->currentCursorPosition()); }
void MainWindow::handleEmergencyStop() {
  controller_.emergencyStop();
  macroController_.emergencyStop();
}
void MainWindow::handleSavePreset() { ClickProfile p = collectProfileFromUi(); settingsRepository_->saveProfile(p); refreshPresetList(p.name); }
void MainWindow::handleNewPreset() {
  bool ok = false; const QString name = QInputDialog::getText(this, "新建配置", "配置名称", QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) return;
  if (settingsRepository_->hasProfile(name)) { QMessageBox::warning(this, "无法新建", "已经存在同名配置。"); return; }
  ClickProfile p = collectProfileFromUi(); p.name = name; settingsRepository_->saveProfile(p); applyProfileToUi(p); refreshPresetList(name);
}
void MainWindow::handleRenamePreset() {
  const QString oldName = selectedProfileName(); if (oldName.isEmpty()) return;
  bool ok = false; const QString name = QInputDialog::getText(this, "重命名配置", "新名称", QLineEdit::Normal, oldName, &ok).trimmed();
  if (ok && settingsRepository_->renameProfile(oldName, name)) { currentProfileName_ = name; refreshPresetList(name); }
}
void MainWindow::handleDeletePreset() {
  const QString name = selectedProfileName(); if (name.isEmpty()) return;
  if (QMessageBox::question(this, "删除配置", QString("确定删除“%1”吗？").arg(name)) == QMessageBox::Yes) {
    settingsRepository_->deleteProfile(name); currentProfileName_ = "Default"; refreshPresetList();
  }
}
void MainWindow::handleLoadPreset() {
  if (const auto p = settingsRepository_->loadProfile(selectedProfileName()); p.has_value()) {
    applyProfileToUi(*p);
    settingsRepository_->saveLastUsedProfile(*p);
    if (globalHotkeysEnabled_) {
      tryEnableGlobalHotkeys(*p);
    } else {
      hotkeyPage_->setActivationState(
          false, "当前未占用任何系统热键");
      macroPage_->setHotkeys(p->hotkeys, false);
    }
  }
}
void MainWindow::handleProfileSelectionChanged() {}
void MainWindow::handlePermissionRequest() {
  backend_->requestAccessibilityPermission();
#if defined(Q_OS_MACOS)
  QDesktopServices::openUrl(QUrl("x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"));
#endif
  updatePermissionBanner();
}
void MainWindow::handleStatusChanged(const QString& status) { statusStrip_->setStatus(status); updatePermissionBanner(); }
void MainWindow::handleRunningChanged(bool running) { updateRunningUi(running); }
void MainWindow::handleCountdownChanged(int seconds) { statusStrip_->setProgress(seconds > 0 ? QString("倒计时 %1 秒").arg(seconds) : "就绪"); }
void MainWindow::handleRemainingClicksChanged(int remaining) { statusStrip_->setProgress(remaining < 0 ? "剩余次数：无限" : QString("剩余 %1 次").arg(remaining)); }
void MainWindow::handleHotkeyActivationRequested(bool enabled) {
  if (!enabled) {
    disableGlobalHotkeys("当前未占用任何系统热键");
    return;
  }
  tryEnableGlobalHotkeys(collectProfileFromUi());
}

bool MainWindow::tryEnableGlobalHotkeys(const ClickProfile& profile) {
  QString error;
  if (!validateHotkeys(profile, &error)) {
    disableGlobalHotkeys(QString("热键无效：%1").arg(error));
    macroPage_->setError(error);
    return false;
  }

  lastHotkeyRegistrationError_.clear();
  globalHotkeysEnabled_ = false;
  if (!hotkeyService_->registerHotkeys(profile)) {
    const QString reason = lastHotkeyRegistrationError_.isEmpty()
                               ? QString("一个或多个热键不可用")
                               : lastHotkeyRegistrationError_;
    disableGlobalHotkeys(QString("启用失败：%1").arg(reason));
    macroPage_->setError(reason);
    return false;
  }

  globalHotkeysEnabled_ = true;
  hotkeyPage_->setActivationState(true, "全局热键已启用");
  macroPage_->setHotkeys(profile.hotkeys, true);
  macroPage_->setError({});
  settingsRepository_->saveLastUsedProfile(profile);
  return true;
}

void MainWindow::disableGlobalHotkeys(const QString& status) {
  hotkeyService_->unregisterAll();
  globalHotkeysEnabled_ = false;
  hotkeyPage_->setActivationState(false, status);
  macroPage_->setHotkeys(collectProfileFromUi().hotkeys, false);
}

void MainWindow::handleMacroRecordRequested(const MacroRecordingOptions& options) {
  if (!macroServices_.recorder || !macroRepository_) {
    macroPage_->setError("键鼠录制服务不可用。");
    return;
  }
  if (options.targetMode == MacroTargetMode::Window && !options.target.nativeId) {
    macroPage_->setError("请先选择一个目标窗口。");
    return;
  }
  if (!confirmMacroSafety()) return;
  QString error;
  if (!macroController_.startRecording(options, &error)) {
    macroPage_->setError(error);
    statusStrip_->setStatus(error);
  } else {
    macroPage_->setError({});
  }
}

void MainWindow::handleMacroPlayRequested(
    const QString& macroId, const MacroPlaybackSettings& settings) {
  const auto saved = findMacro(macroId);
  if (!saved) {
    macroPage_->setError("请先选择一个已保存的宏。");
    return;
  }
  MacroSequence sequence = *saved;
  sequence.playback = settings;
  sequence.modifiedAt = QDateTime::currentDateTimeUtc();
  QString error;
  if (!macroRepository_->save(sequence, &error) ||
      !macroController_.startPlayback(sequence, &error)) {
    macroPage_->setError(error);
    statusStrip_->setStatus(error);
  } else {
    macroPage_->setError({});
  }
}

void MainWindow::handleMacroStopRequested() {
  macroController_.stop();
}

void MainWindow::handleMacroRecordingCompleted(const MacroSequence& sequence) {
  if (!macroRepository_) return;
  MacroSequence saved = sequence;
  saved.name = macroNameProvider_(this).trimmed();
  if (saved.name.isEmpty()) {
    statusStrip_->setStatus("录制未保存");
    return;
  }
  saved.modifiedAt = QDateTime::currentDateTimeUtc();
  QString error;
  if (!macroRepository_->save(saved, &error)) {
    macroPage_->setError(error);
    return;
  }
  refreshMacroList(saved.id);
  statusStrip_->setStatus("宏已保存");
}

void MainWindow::handleMacroDeleteRequested(const QString& macroId) {
  const auto sequence = findMacro(macroId);
  if (!sequence || !macroRepository_) return;
  if (QMessageBox::question(this, "删除宏",
                            QString("确定删除“%1”吗？").arg(sequence->name)) !=
      QMessageBox::Yes) {
    return;
  }
  QString error;
  if (!macroRepository_->remove(macroId, &error)) macroPage_->setError(error);
  refreshMacroList();
}

void MainWindow::handleMacroRenameRequested(const QString& macroId) {
  const auto sequence = findMacro(macroId);
  if (!sequence || !macroRepository_) return;
  bool accepted = false;
  const QString name = QInputDialog::getText(
                           this, "重命名宏", "新名称", QLineEdit::Normal,
                           sequence->name, &accepted)
                           .trimmed();
  if (!accepted || name.isEmpty()) return;
  QString error;
  if (!macroRepository_->rename(macroId, name, &error)) macroPage_->setError(error);
  refreshMacroList(macroId);
}

void MainWindow::handleMacroWindowPointSelected(const QPoint& globalPoint) {
  if (!macroServices_.windowService) return;
  const auto target = macroServices_.windowService->windowAt(globalPoint);
  if (!target || target->nativeId == static_cast<quintptr>(winId())) {
    macroPage_->setError("没有选中可录制的目标窗口。");
    return;
  }
  refreshMacroWindows(target->nativeId);
}

void MainWindow::handleMacroStateChanged(MacroControllerState state) {
  MacroPageActivity pageActivity = MacroPageActivity::Idle;
  if (state == MacroControllerState::Recording) pageActivity = MacroPageActivity::Recording;
  if (state == MacroControllerState::Playing) pageActivity = MacroPageActivity::Playing;
  macroPage_->setActivity(pageActivity);

  const bool idle = state == MacroControllerState::Idle;
  clickPage_->setEditingEnabled(idle && !controller_.isRunning());
  hotkeyPage_->setEditingEnabled(idle && !controller_.isRunning());
  presetsPage_->setMutationEnabled(idle && !controller_.isRunning());
  actionBar_->setEnabled(idle);

  const ClickProfile profile = collectProfileFromUi();
  if (state == MacroControllerState::Recording) {
    statusStrip_->setProgress(
        QString("%1 停止 · %2 紧急停止")
            .arg(profile.hotkeys.macroRecord, profile.hotkeys.emergencyStop));
  } else if (state == MacroControllerState::Playing) {
    statusStrip_->setProgress(
        QString("%1 停止 · %2 紧急停止")
            .arg(profile.hotkeys.macroPlayback, profile.hotkeys.emergencyStop));
  } else {
    statusStrip_->setProgress("就绪");
  }
}
void MainWindow::refreshPresetList(const QString& selected) {
  QStringList names = settingsRepository_->profileNames(); names.sort(Qt::CaseInsensitive); presetsPage_->setPresetNames(names, selected);
}
void MainWindow::applyProfileToUi(const ClickProfile& p) {
  currentProfileName_ = p.name; clickPage_->setProfile(p); hotkeyPage_->setProfile(p);
  macroPage_->setHotkeys(p.hotkeys, false);
  actionBar_->setSummary(clickPage_->summary()); applyWindowOnTop(p.alwaysOnTop);
}
ClickProfile MainWindow::collectProfileFromUi() const {
  ClickProfile p; p.name = currentProfileName_; clickPage_->applyToProfile(p); hotkeyPage_->applyToProfile(p); return p;
}
void MainWindow::updateRunningUi(bool running) {
  clickPage_->setEditingEnabled(!running); hotkeyPage_->setEditingEnabled(!running);
  presetsPage_->setMutationEnabled(!running); actionBar_->setRunning(running);
  actionBar_->setEnabled(true);
  if (macroServices_.recorder && macroRepository_) {
    macroPage_->setActivity(running ? MacroPageActivity::Unavailable
                                    : MacroPageActivity::Idle);
    if (running) macroPage_->setError("连点运行中，键鼠录制与回放暂不可用。");
  }
}
void MainWindow::updatePermissionBanner() { statusStrip_->setPermissionState(backend_->hasAccessibilityPermission()); }
void MainWindow::applyWindowOnTop(bool enabled) { const bool shown = isVisible(); setWindowFlag(Qt::WindowStaysOnTopHint, enabled); if (shown) { show(); raise(); } }
bool MainWindow::validateHotkeys(const ClickProfile& profile, QString* error) const { return hotkeyPage_->validate(profile, error); }
QString MainWindow::selectedProfileName() const { return presetsPage_->selectedPresetName(); }

void MainWindow::refreshMacroList(const QString& selectedId) {
  if (!macroRepository_) {
    macros_.clear();
    macroPage_->setMacros({});
    return;
  }
  QStringList warnings;
  macros_ = macroRepository_->loadAll(&warnings);
  macroPage_->setMacros(macros_, selectedId);
  if (!warnings.isEmpty()) macroPage_->setError(warnings.first());
}

void MainWindow::refreshMacroWindows(quintptr selectedNativeId) {
  if (!macroServices_.windowService) {
    macroPage_->setAvailableWindows({});
    return;
  }
  QVector<WindowTarget> windows = macroServices_.windowService->availableWindows();
  const quintptr ownWindow = static_cast<quintptr>(winId());
  windows.erase(std::remove_if(windows.begin(), windows.end(),
                               [ownWindow](const WindowTarget& target) {
                                 return target.nativeId == ownWindow;
                               }),
                windows.end());
  macroPage_->setAvailableWindows(windows, selectedNativeId);
}

std::optional<MacroSequence> MainWindow::findMacro(const QString& id) const {
  const auto match = std::find_if(macros_.cbegin(), macros_.cend(),
                                  [&id](const MacroSequence& sequence) {
                                    return sequence.id == id;
                                  });
  return match == macros_.cend() ? std::nullopt
                                 : std::optional<MacroSequence>(*match);
}

bool MainWindow::confirmMacroSafety() {
  if (settingsRepository_->macroSafetyAcknowledged()) return true;
  if (!safetyConfirmation_(this)) return false;
  settingsRepository_->setMacroSafetyAcknowledged(true);
  return true;
}
