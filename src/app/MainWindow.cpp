#include "app/MainWindow.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

#include "app/pages/ClickSettingsPage.h"
#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/PresetsAboutPage.h"
#include "app/widgets/ActionBar.h"
#include "app/widgets/NavigationSidebar.h"
#include "app/widgets/StatusStrip.h"
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"

MainWindow::MainWindow(QWidget* parent)
    : MainWindow(createClickBackend(), createHotkeyService(),
                 std::make_unique<SettingsRepository>(), parent) {}

MainWindow::MainWindow(std::unique_ptr<ClickBackend> backend,
                       std::unique_ptr<HotkeyService> hotkeyService,
                       std::unique_ptr<SettingsRepository> settingsRepository,
                       QWidget* parent)
    : QMainWindow(parent),
      backend_(std::move(backend)),
      hotkeyService_(std::move(hotkeyService)),
      settingsRepository_(std::move(settingsRepository)),
      controller_(backend_.get(), this) {
  buildUi();

  connect(actionBar_, &ActionBar::startStopRequested, this, &MainWindow::handleStartStop);
  connect(clickPage_, &ClickSettingsPage::captureRequested, this, &MainWindow::handleCapturePoint);
  connect(clickPage_, &ClickSettingsPage::alwaysOnTopChanged, this, &MainWindow::applyWindowOnTop);
  connect(clickPage_, &ClickSettingsPage::settingsChanged, this,
          [this] { actionBar_->setSummary(clickPage_->summary()); });
  connect(sidebar_, &NavigationSidebar::pageSelected, this,
          [this](ShellPage page) { pages_->setCurrentIndex(static_cast<int>(page)); });

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

  connect(hotkeyService_.get(), &HotkeyService::startStopPressed, this, &MainWindow::handleStartStop);
  connect(hotkeyService_.get(), &HotkeyService::capturePointPressed, this, &MainWindow::handleCapturePoint);
  connect(hotkeyService_.get(), &HotkeyService::emergencyStopPressed, this, &MainWindow::handleEmergencyStop);
  connect(hotkeyService_.get(), &HotkeyService::registrationFailed, this,
          [this](const QString& message) { QMessageBox::warning(this, "热键注册失败", message); });

  ClickProfile profile;
  if (const auto saved = settingsRepository_->loadLastUsedProfile(); saved.has_value())
    profile = *saved;
  applyProfileToUi(profile);
  refreshPresetList(profile.name);
  updatePermissionBanner();
  hotkeyService_->registerHotkeys(collectProfileFromUi());
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
  hotkeyPage_ = new HotkeySettingsPage(pages_);
  presetsPage_ = new PresetsAboutPage(pages_);
  pages_->addWidget(clickPage_);
  pages_->addWidget(hotkeyPage_);
  pages_->addWidget(presetsPage_);
  actionBar_ = new ActionBar(content);
  layout->addWidget(statusStrip_);
  layout->addWidget(pages_, 1);
  layout->addWidget(actionBar_);
  shell->addWidget(content, 1);

  setWindowTitle("ClickFlow");
  setMinimumSize(820, 560);
  resize(920, 620);
  setStyleSheet(R"(
    QMainWindow, #contentSurface { background: #f4f5f7; color: #18202b; }
    #navigationSidebar { background: #e9ecf1; border-right: 1px solid #d4d9e1; }
    #productName { font-size: 22px; font-weight: 700; color: #14213d; }
    #productVersion { color: #6b7280; }
    #sidebarNavigation {
      background: transparent; border: none; outline: none;
    }
    #sidebarNavigation::item { border-radius: 8px; padding-left: 12px; }
    #sidebarNavigation::item:selected { background: #2563eb; color: white; }
    #settingsCard, #statusStrip, #actionBar {
      background: white; border: 1px solid #dfe3e8; border-radius: 10px;
    }
    #cardTitle { font-size: 16px; font-weight: 650; }
    QPushButton#startStopButton {
      background: #2563eb; color: white; border: 0; border-radius: 8px;
      padding: 10px 22px; font-weight: 650;
    }
    QPushButton#startStopButton[running="true"] { background: #dc2626; }
    QComboBox, QSpinBox, QKeySequenceEdit {
      min-height: 30px; border: 1px solid #cfd5dd; border-radius: 7px;
      background: white; padding: 2px 34px 2px 10px;
    }
    QComboBox:hover, QSpinBox:hover, QKeySequenceEdit:hover {
      border-color: #9eabc0;
    }
    QComboBox:focus, QSpinBox:focus, QKeySequenceEdit:focus {
      border: 1px solid #2563eb;
    }
    QComboBox::drop-down {
      subcontrol-origin: padding; subcontrol-position: top right;
      width: 30px; margin: 3px; border: none; border-radius: 5px;
    }
    QComboBox::drop-down:hover { background: #edf3ff; }
    QComboBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.svg);
      width: 12px; height: 8px;
    }
    QSpinBox { padding-right: 32px; }
    QSpinBox::up-button, QSpinBox::down-button {
      subcontrol-origin: border; width: 28px;
      border: none; background: transparent;
    }
    QSpinBox::up-button {
      subcontrol-position: top right; margin: 3px 3px 0 0;
      border-top-left-radius: 5px; border-top-right-radius: 5px;
    }
    QSpinBox::down-button {
      subcontrol-position: bottom right; margin: 0 3px 3px 0;
      border-bottom-left-radius: 5px; border-bottom-right-radius: 5px;
    }
    QSpinBox::up-button:hover, QSpinBox::down-button:hover {
      background: #edf3ff;
    }
    QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
      background: #dce8ff;
    }
    QSpinBox::up-arrow {
      image: url(:/clickflow/icons/chevron-up.svg);
      width: 10px; height: 6px;
    }
    QSpinBox::down-arrow {
      image: url(:/clickflow/icons/chevron-down.svg);
      width: 10px; height: 6px;
    }
    QComboBox:disabled, QSpinBox:disabled, QKeySequenceEdit:disabled {
      color: #8a94a3; background: #f5f6f8;
    }
  )");
}

void MainWindow::handleStartStop() {
  if (controller_.isRunning()) { controller_.stop(); return; }
  QString error;
  if (!validateHotkeys(&error)) { QMessageBox::warning(this, "热键设置无效", error); return; }
  ClickProfile profile = collectProfileFromUi();
  if (!hotkeyService_->registerHotkeys(profile)) return;
  settingsRepository_->saveLastUsedProfile(profile);
  controller_.start(profile);
}
void MainWindow::handleCapturePoint() { clickPage_->setFixedPoint(backend_->currentCursorPosition()); }
void MainWindow::handleEmergencyStop() { controller_.emergencyStop(); }
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
  if (const auto p = settingsRepository_->loadProfile(selectedProfileName()); p.has_value()) { applyProfileToUi(*p); settingsRepository_->saveLastUsedProfile(*p); }
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
void MainWindow::refreshPresetList(const QString& selected) {
  QStringList names = settingsRepository_->profileNames(); names.sort(Qt::CaseInsensitive); presetsPage_->setPresetNames(names, selected);
}
void MainWindow::applyProfileToUi(const ClickProfile& p) {
  currentProfileName_ = p.name; clickPage_->setProfile(p); hotkeyPage_->setProfile(p);
  actionBar_->setSummary(clickPage_->summary()); applyWindowOnTop(p.alwaysOnTop);
}
ClickProfile MainWindow::collectProfileFromUi() const {
  ClickProfile p; p.name = currentProfileName_; clickPage_->applyToProfile(p); hotkeyPage_->applyToProfile(p); return p;
}
void MainWindow::updateRunningUi(bool running) {
  clickPage_->setEditingEnabled(!running); hotkeyPage_->setEditingEnabled(!running);
  presetsPage_->setMutationEnabled(!running); actionBar_->setRunning(running);
}
void MainWindow::updatePermissionBanner() { statusStrip_->setPermissionState(backend_->hasAccessibilityPermission()); }
void MainWindow::applyWindowOnTop(bool enabled) { const bool shown = isVisible(); setWindowFlag(Qt::WindowStaysOnTopHint, enabled); if (shown) { show(); raise(); } }
bool MainWindow::validateHotkeys(QString* error) const { return hotkeyPage_->validate(error); }
QString MainWindow::selectedProfileName() const { return presetsPage_->selectedPresetName(); }
