#include "app/MainWindow.h"

#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

#include "core/ClickBackend.h"
#include "core/HotkeyService.h"
#include "core/ClickTypes.h"

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

  connect(startStopButton_, &QPushButton::clicked, this, &MainWindow::handleStartStop);
  connect(permissionButton_, &QPushButton::clicked, this, &MainWindow::handlePermissionRequest);

  connect(&controller_, &ClickController::statusChanged, this, &MainWindow::handleStatusChanged);
  connect(&controller_, &ClickController::runningChanged, this,
          &MainWindow::handleRunningChanged);
  connect(&controller_, &ClickController::countdownChanged, this,
          &MainWindow::handleCountdownChanged);
  connect(&controller_, &ClickController::remainingClicksChanged, this,
          &MainWindow::handleRemainingClicksChanged);
  connect(&controller_, &ClickController::startRejected, this,
          [this](const QString& reason) { QMessageBox::warning(this, "无法启动", reason); });

  connect(hotkeyService_.get(), &HotkeyService::startStopPressed, this,
          &MainWindow::handleStartStop);
  connect(hotkeyService_.get(), &HotkeyService::capturePointPressed, this,
          &MainWindow::handleCapturePoint);
  connect(hotkeyService_.get(), &HotkeyService::emergencyStopPressed, this,
          &MainWindow::handleEmergencyStop);
  connect(hotkeyService_.get(), &HotkeyService::registrationFailed, this,
          [this](const QString& message) {
            QMessageBox::warning(this, "热键注册失败", message);
          });

  ClickProfile profile;
  if (const auto saved = settingsRepository_->loadLastUsedProfile(); saved.has_value()) {
    profile = *saved;
  }
  applyProfileToUi(profile);
  updatePermissionBanner();
  hotkeyService_->registerHotkeys(collectProfileFromUi());

  setWindowTitle("极简连点器");
  resize(640, 320);
}

MainWindow::~MainWindow() = default;

void MainWindow::handleStartStop() {
  if (controller_.isRunning()) {
    controller_.stop();
    return;
  }

  QString hotkeyError;
  if (!validateHotkeys(&hotkeyError)) {
    QMessageBox::warning(this, "热键设置无效", hotkeyError);
    return;
  }

  ClickProfile profile = collectProfileFromUi();
  if (!hotkeyService_->registerHotkeys(profile)) {
    return;
  }

  settingsRepository_->saveLastUsedProfile(profile);
  controller_.start(profile);
}

void MainWindow::handleCapturePoint() {
  Q_UNUSED(this);
}

void MainWindow::handleEmergencyStop() {
  controller_.emergencyStop();
}

void MainWindow::handleSavePreset() {
}

void MainWindow::handleNewPreset() {
}

void MainWindow::handleRenamePreset() {
}

void MainWindow::handleDeletePreset() {
}

void MainWindow::handleLoadPreset() {
}

void MainWindow::handleProfileSelectionChanged() {
}

void MainWindow::handlePermissionRequest() {
  backend_->requestAccessibilityPermission();
#if defined(Q_OS_MACOS)
  QDesktopServices::openUrl(
      QUrl("x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"));
#endif
  updatePermissionBanner();
}

void MainWindow::handleStatusChanged(const QString& status) {
  statusLabel_->setText(status);
  updatePermissionBanner();
}

void MainWindow::handleRunningChanged(bool running) {
  updateRunningUi(running);
}

void MainWindow::handleCountdownChanged(int seconds) {
  countdownLabel_->setText(seconds > 0 ? QString("倒计时：%1 秒").arg(seconds)
                                       : "倒计时：就绪");
}

void MainWindow::handleRemainingClicksChanged(int remaining) {
  remainingLabel_->setText(remaining < 0 ? "剩余次数：无限"
                                         : QString("剩余次数：%1").arg(remaining));
}

void MainWindow::buildUi() {
  auto* central = new QWidget(this);
  setCentralWidget(central);

  auto* rootLayout = new QVBoxLayout(central);

  auto* guideBox = new QGroupBox("怎么用", central);
  auto* guideLayout = new QHBoxLayout(guideBox);
  quickStartLabel_ = new QLabel(
      "1. 把鼠标移到你要连点的位置\n"
      "2. 按 F6 开始连点\n"
      "3. 再按一次 F6 停止连点\n"
      "4. 如果没反应，先确认已经授予辅助功能权限",
      guideBox);
  quickStartLabel_->setWordWrap(true);
  guideLayout->addWidget(quickStartLabel_, 1);
  rootLayout->addWidget(guideBox);

  auto* headerBox = new QGroupBox("运行状态", central);
  auto* headerLayout = new QHBoxLayout(headerBox);
  permissionLabel_ = new QLabel(headerBox);
  permissionLabel_->setObjectName("permissionLabel");
  permissionButton_ = new QPushButton("打开辅助功能设置", headerBox);
  permissionButton_->setObjectName("permissionButton");
  statusLabel_ = new QLabel("空闲", headerBox);
  countdownLabel_ = new QLabel("模式：按一次开始，再按一次停止", headerBox);
  remainingLabel_ = new QLabel("热键：F6", headerBox);
  headerLayout->addWidget(permissionLabel_, 2);
  headerLayout->addWidget(permissionButton_);
  headerLayout->addWidget(statusLabel_);
  headerLayout->addWidget(countdownLabel_);
  headerLayout->addWidget(remainingLabel_);
  rootLayout->addWidget(headerBox);

  configPanel_ = new QWidget(central);
  auto* configLayout = new QVBoxLayout(configPanel_);

  clickGroup_ = new QGroupBox("基础设置", configPanel_);
  auto* clickForm = new QFormLayout(clickGroup_);
  intervalSpin_ = new QSpinBox(clickGroup_);
  intervalSpin_->setRange(1, 600000);
  intervalSpin_->setSuffix(" 毫秒");
  clickForm->addRow("点击间隔", intervalSpin_);
  configLayout->addWidget(clickGroup_);

  hotkeyGroup_ = new QGroupBox("热键设置", configPanel_);
  auto* hotkeyForm = new QFormLayout(hotkeyGroup_);
  startStopHotkeyEdit_ = new QKeySequenceEdit(hotkeyGroup_);
  hotkeyForm->addRow("开始 / 停止热键", startStopHotkeyEdit_);
  configLayout->addWidget(hotkeyGroup_);

  auto* runButtonsLayout = new QHBoxLayout();
  startStopButton_ = new QPushButton("开始连点", configPanel_);
  runButtonsLayout->addWidget(startStopButton_);
  configLayout->addLayout(runButtonsLayout);

  rootLayout->addWidget(configPanel_);
}

void MainWindow::refreshPresetList(const QString& selectedName) {
  Q_UNUSED(selectedName);
}

void MainWindow::applyProfileToUi(const ClickProfile& profile) {
  intervalSpin_->setValue(profile.intervalMs);
  startStopHotkeyEdit_->setKeySequence(
      QKeySequence::fromString(profile.hotkeys.startStop, QKeySequence::PortableText));
}

ClickProfile MainWindow::collectProfileFromUi() const {
  ClickProfile profile;
  profile.name = "极简模式";
  profile.intervalMs = intervalSpin_->value();
  profile.button = ClickButton::Left;
  profile.targetMode = TargetMode::FollowCursor;
  profile.fixedPoint = QPoint(0, 0);
  profile.repeatMode = RepeatMode::Infinite;
  profile.repeatCount = 1;
  profile.jitterRadius = 0;
  profile.countdownSeconds = 0;
  profile.alwaysOnTop = false;
  profile.hotkeys.startStop =
      startStopHotkeyEdit_->keySequence().toString(QKeySequence::PortableText);
  profile.hotkeys.capturePoint = "F7";
  profile.hotkeys.emergencyStop = "F8";
  return profile;
}

void MainWindow::updateTargetModeUi() {
}

void MainWindow::updateRepeatModeUi() {
}

void MainWindow::updateRunningUi(bool running) {
  clickGroup_->setEnabled(!running);
  hotkeyGroup_->setEnabled(!running);
  startStopButton_->setText(running ? "停止连点" : "开始连点");
}

void MainWindow::updatePermissionBanner() {
  const bool allowed = backend_->hasAccessibilityPermission();
  permissionLabel_->setText(allowed ? "输入控制权限：可用"
                                    : "输入控制权限：需要授权");
  permissionButton_->setVisible(!allowed);
}

void MainWindow::applyWindowOnTop(bool enabled) {
  const bool visible = isVisible();
  setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
  if (visible) {
    show();
    raise();
  }
}

bool MainWindow::validateHotkeys(QString* errorMessage) const {
  const QString startStop =
      startStopHotkeyEdit_->keySequence().toString(QKeySequence::PortableText);

  if (startStop.isEmpty()) {
    *errorMessage = "请设置一个开始/停止热键。";
    return false;
  }

  if (startStop == "F7" || startStop == "F8") {
    *errorMessage = "开始/停止热键不要和保留热键 F7、F8 冲突。";
    return false;
  }

  return true;
}

QString MainWindow::selectedProfileName() const {
  return "极简模式";
}
