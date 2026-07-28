#include "app/MainWindow.h"

#include <QDesktopServices>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
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
  connect(capturePointButton_, &QPushButton::clicked, this, &MainWindow::handleCapturePoint);
  connect(newPresetButton_, &QPushButton::clicked, this, &MainWindow::handleNewPreset);
  connect(savePresetButton_, &QPushButton::clicked, this, &MainWindow::handleSavePreset);
  connect(renamePresetButton_, &QPushButton::clicked, this, &MainWindow::handleRenamePreset);
  connect(deletePresetButton_, &QPushButton::clicked, this, &MainWindow::handleDeletePreset);
  connect(loadPresetButton_, &QPushButton::clicked, this, &MainWindow::handleLoadPreset);
  connect(profileList_, &QListWidget::itemSelectionChanged, this,
          &MainWindow::handleProfileSelectionChanged);
  connect(targetModeCombo_, &QComboBox::currentIndexChanged, this,
          &MainWindow::updateTargetModeUi);
  connect(repeatModeCombo_, &QComboBox::currentIndexChanged, this,
          &MainWindow::updateRepeatModeUi);
  connect(alwaysOnTopCheck_, &QCheckBox::toggled, this, &MainWindow::applyWindowOnTop);

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
  refreshPresetList(profile.name);
  updatePermissionBanner();
  hotkeyService_->registerHotkeys(collectProfileFromUi());

  setWindowTitle("极简连点器");
  resize(760, 760);
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
  const QPoint point = backend_->currentCursorPosition();
  fixedXSpin_->setValue(point.x());
  fixedYSpin_->setValue(point.y());
  targetModeCombo_->setCurrentIndex(
      targetModeCombo_->findData(static_cast<int>(TargetMode::FixedPoint)));
}

void MainWindow::handleEmergencyStop() {
  controller_.emergencyStop();
}

void MainWindow::handleSavePreset() {
  ClickProfile profile = collectProfileFromUi();
  if (profile.name.trimmed().isEmpty()) {
    QMessageBox::warning(this, "无法保存", "配置名称不能为空。");
    return;
  }
  settingsRepository_->saveProfile(profile);
  refreshPresetList(profile.name);
}

void MainWindow::handleNewPreset() {
  bool accepted = false;
  const QString name =
      QInputDialog::getText(this, "新建配置", "配置名称", QLineEdit::Normal,
                            QString(), &accepted)
          .trimmed();
  if (!accepted || name.isEmpty()) {
    return;
  }
  if (settingsRepository_->hasProfile(name)) {
    QMessageBox::warning(this, "无法新建", "已经存在同名配置。");
    return;
  }

  ClickProfile profile = collectProfileFromUi();
  profile.name = name;
  settingsRepository_->saveProfile(profile);
  applyProfileToUi(profile);
  refreshPresetList(name);
}

void MainWindow::handleRenamePreset() {
  const QString oldName = selectedProfileName();
  if (oldName.isEmpty()) {
    return;
  }
  bool accepted = false;
  const QString newName =
      QInputDialog::getText(this, "重命名配置", "新名称", QLineEdit::Normal,
                            oldName, &accepted)
          .trimmed();
  if (!accepted || newName.isEmpty()) {
    return;
  }
  if (!settingsRepository_->renameProfile(oldName, newName)) {
    QMessageBox::warning(this, "无法重命名", "名称无效或已经存在同名配置。");
    return;
  }
  currentProfileName_ = newName;
  refreshPresetList(newName);
}

void MainWindow::handleDeletePreset() {
  const QString name = selectedProfileName();
  if (name.isEmpty()) {
    return;
  }
  if (QMessageBox::question(this, "删除配置",
                            QString("确定删除“%1”吗？").arg(name)) !=
      QMessageBox::Yes) {
    return;
  }
  settingsRepository_->deleteProfile(name);
  currentProfileName_ = "Default";
  refreshPresetList();
}

void MainWindow::handleLoadPreset() {
  const QString name = selectedProfileName();
  if (name.isEmpty()) {
    return;
  }
  const auto profile = settingsRepository_->loadProfile(name);
  if (profile.has_value()) {
    applyProfileToUi(*profile);
    settingsRepository_->saveLastUsedProfile(*profile);
  }
}

void MainWindow::handleProfileSelectionChanged() {
  const bool selected = !selectedProfileName().isEmpty();
  loadPresetButton_->setEnabled(selected);
  renamePresetButton_->setEnabled(selected);
  deletePresetButton_->setEnabled(selected);
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
      "4. 按 F7 捕获固定坐标，按 F8 紧急停止",
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
  configPanel_->setObjectName("configPanel");
  auto* configLayout = new QVBoxLayout(configPanel_);

  profileGroup_ = new QGroupBox("配置预设", configPanel_);
  auto* profileLayout = new QHBoxLayout(profileGroup_);
  profileList_ = new QListWidget(profileGroup_);
  profileList_->setObjectName("profileList");
  profileLayout->addWidget(profileList_, 1);
  auto* profileButtons = new QVBoxLayout();
  newPresetButton_ = new QPushButton("新建", profileGroup_);
  savePresetButton_ = new QPushButton("保存", profileGroup_);
  renamePresetButton_ = new QPushButton("重命名", profileGroup_);
  deletePresetButton_ = new QPushButton("删除", profileGroup_);
  loadPresetButton_ = new QPushButton("加载", profileGroup_);
  profileButtons->addWidget(newPresetButton_);
  profileButtons->addWidget(savePresetButton_);
  profileButtons->addWidget(renamePresetButton_);
  profileButtons->addWidget(deletePresetButton_);
  profileButtons->addWidget(loadPresetButton_);
  profileButtons->addStretch();
  profileLayout->addLayout(profileButtons);
  configLayout->addWidget(profileGroup_);

  clickGroup_ = new QGroupBox("基础设置", configPanel_);
  auto* clickForm = new QFormLayout(clickGroup_);
  intervalSpin_ = new QSpinBox(clickGroup_);
  intervalSpin_->setObjectName("intervalSpin");
  intervalSpin_->setRange(1, 600000);
  intervalSpin_->setSuffix(" 毫秒");
  clickForm->addRow("点击间隔", intervalSpin_);

  buttonCombo_ = new QComboBox(clickGroup_);
  buttonCombo_->setObjectName("buttonCombo");
  buttonCombo_->addItem("左键", static_cast<int>(ClickButton::Left));
  buttonCombo_->addItem("右键", static_cast<int>(ClickButton::Right));
  clickForm->addRow("鼠标按键", buttonCombo_);

  targetModeCombo_ = new QComboBox(clickGroup_);
  targetModeCombo_->setObjectName("targetModeCombo");
  targetModeCombo_->addItem("跟随当前鼠标",
                            static_cast<int>(TargetMode::FollowCursor));
  targetModeCombo_->addItem("固定坐标", static_cast<int>(TargetMode::FixedPoint));
  clickForm->addRow("点击位置", targetModeCombo_);

  auto* fixedPointWidget = new QWidget(clickGroup_);
  auto* fixedPointLayout = new QHBoxLayout(fixedPointWidget);
  fixedPointLayout->setContentsMargins(0, 0, 0, 0);
  fixedXSpin_ = new QSpinBox(fixedPointWidget);
  fixedXSpin_->setObjectName("fixedXSpin");
  fixedXSpin_->setRange(-100000, 100000);
  fixedXSpin_->setPrefix("X ");
  fixedYSpin_ = new QSpinBox(fixedPointWidget);
  fixedYSpin_->setObjectName("fixedYSpin");
  fixedYSpin_->setRange(-100000, 100000);
  fixedYSpin_->setPrefix("Y ");
  capturePointButton_ = new QPushButton("捕获当前坐标（F7）", fixedPointWidget);
  capturePointButton_->setObjectName("capturePointButton");
  fixedPointLayout->addWidget(fixedXSpin_);
  fixedPointLayout->addWidget(fixedYSpin_);
  fixedPointLayout->addWidget(capturePointButton_);
  clickForm->addRow("固定坐标", fixedPointWidget);

  jitterSpin_ = new QSpinBox(clickGroup_);
  jitterSpin_->setObjectName("jitterSpin");
  jitterSpin_->setRange(0, 1000);
  jitterSpin_->setSuffix(" 像素");
  clickForm->addRow("随机抖动半径", jitterSpin_);

  repeatModeCombo_ = new QComboBox(clickGroup_);
  repeatModeCombo_->setObjectName("repeatModeCombo");
  repeatModeCombo_->addItem("无限", static_cast<int>(RepeatMode::Infinite));
  repeatModeCombo_->addItem("有限次数", static_cast<int>(RepeatMode::Finite));
  clickForm->addRow("重复方式", repeatModeCombo_);

  repeatCountSpin_ = new QSpinBox(clickGroup_);
  repeatCountSpin_->setObjectName("repeatCountSpin");
  repeatCountSpin_->setRange(1, 100000000);
  repeatCountSpin_->setSuffix(" 次");
  clickForm->addRow("点击次数", repeatCountSpin_);

  countdownSpin_ = new QSpinBox(clickGroup_);
  countdownSpin_->setObjectName("countdownSpin");
  countdownSpin_->setRange(0, 3600);
  countdownSpin_->setSuffix(" 秒");
  clickForm->addRow("开始前倒计时", countdownSpin_);

  alwaysOnTopCheck_ = new QCheckBox("窗口保持置顶", clickGroup_);
  alwaysOnTopCheck_->setObjectName("alwaysOnTopCheck");
  clickForm->addRow("窗口", alwaysOnTopCheck_);
  configLayout->addWidget(clickGroup_);

  hotkeyGroup_ = new QGroupBox("热键设置", configPanel_);
  auto* hotkeyForm = new QFormLayout(hotkeyGroup_);
  startStopHotkeyEdit_ = new QKeySequenceEdit(hotkeyGroup_);
  startStopHotkeyEdit_->setObjectName("startStopHotkeyEdit");
  hotkeyForm->addRow("开始 / 停止热键", startStopHotkeyEdit_);
  captureHotkeyEdit_ = new QKeySequenceEdit(hotkeyGroup_);
  captureHotkeyEdit_->setObjectName("captureHotkeyEdit");
  hotkeyForm->addRow("捕获坐标热键", captureHotkeyEdit_);
  emergencyHotkeyEdit_ = new QKeySequenceEdit(hotkeyGroup_);
  emergencyHotkeyEdit_->setObjectName("emergencyHotkeyEdit");
  hotkeyForm->addRow("紧急停止热键", emergencyHotkeyEdit_);
  configLayout->addWidget(hotkeyGroup_);

  startStopButton_ = new QPushButton("开始连点", configPanel_);
  startStopButton_->setObjectName("startStopButton");

  rootLayout->addWidget(configPanel_);
  rootLayout->addWidget(startStopButton_);
}

void MainWindow::refreshPresetList(const QString& selectedName) {
  const QSignalBlocker blocker(profileList_);
  profileList_->clear();
  QStringList names = settingsRepository_->profileNames();
  names.sort(Qt::CaseInsensitive);
  profileList_->addItems(names);

  const auto matches =
      profileList_->findItems(selectedName, Qt::MatchExactly);
  if (!matches.isEmpty()) {
    profileList_->setCurrentItem(matches.first());
  }

  const bool selected = profileList_->currentItem() != nullptr;
  loadPresetButton_->setEnabled(selected);
  renamePresetButton_->setEnabled(selected);
  deletePresetButton_->setEnabled(selected);
}

void MainWindow::applyProfileToUi(const ClickProfile& profile) {
  currentProfileName_ = profile.name;
  intervalSpin_->setValue(profile.intervalMs);
  buttonCombo_->setCurrentIndex(
      buttonCombo_->findData(static_cast<int>(profile.button)));
  targetModeCombo_->setCurrentIndex(
      targetModeCombo_->findData(static_cast<int>(profile.targetMode)));
  fixedXSpin_->setValue(profile.fixedPoint.x());
  fixedYSpin_->setValue(profile.fixedPoint.y());
  jitterSpin_->setValue(profile.jitterRadius);
  repeatModeCombo_->setCurrentIndex(
      repeatModeCombo_->findData(static_cast<int>(profile.repeatMode)));
  repeatCountSpin_->setValue(profile.repeatCount);
  countdownSpin_->setValue(profile.countdownSeconds);
  alwaysOnTopCheck_->setChecked(profile.alwaysOnTop);
  startStopHotkeyEdit_->setKeySequence(
      QKeySequence::fromString(profile.hotkeys.startStop, QKeySequence::PortableText));
  captureHotkeyEdit_->setKeySequence(
      QKeySequence::fromString(profile.hotkeys.capturePoint,
                               QKeySequence::PortableText));
  emergencyHotkeyEdit_->setKeySequence(
      QKeySequence::fromString(profile.hotkeys.emergencyStop,
                               QKeySequence::PortableText));
  updateTargetModeUi();
  updateRepeatModeUi();
  applyWindowOnTop(profile.alwaysOnTop);
}

ClickProfile MainWindow::collectProfileFromUi() const {
  ClickProfile profile;
  profile.name = currentProfileName_;
  profile.intervalMs = intervalSpin_->value();
  profile.button = static_cast<ClickButton>(buttonCombo_->currentData().toInt());
  profile.targetMode =
      static_cast<TargetMode>(targetModeCombo_->currentData().toInt());
  profile.fixedPoint = QPoint(fixedXSpin_->value(), fixedYSpin_->value());
  profile.repeatMode =
      static_cast<RepeatMode>(repeatModeCombo_->currentData().toInt());
  profile.repeatCount = repeatCountSpin_->value();
  profile.jitterRadius = jitterSpin_->value();
  profile.countdownSeconds = countdownSpin_->value();
  profile.alwaysOnTop = alwaysOnTopCheck_->isChecked();
  profile.hotkeys.startStop =
      startStopHotkeyEdit_->keySequence().toString(QKeySequence::PortableText);
  profile.hotkeys.capturePoint =
      captureHotkeyEdit_->keySequence().toString(QKeySequence::PortableText);
  profile.hotkeys.emergencyStop =
      emergencyHotkeyEdit_->keySequence().toString(QKeySequence::PortableText);
  return profile;
}

void MainWindow::updateTargetModeUi() {
  const bool fixed = static_cast<TargetMode>(targetModeCombo_->currentData().toInt()) ==
                     TargetMode::FixedPoint;
  fixedXSpin_->setEnabled(fixed);
  fixedYSpin_->setEnabled(fixed);
  capturePointButton_->setEnabled(fixed);
}

void MainWindow::updateRepeatModeUi() {
  const bool finite =
      static_cast<RepeatMode>(repeatModeCombo_->currentData().toInt()) ==
      RepeatMode::Finite;
  repeatCountSpin_->setEnabled(finite);
}

void MainWindow::updateRunningUi(bool running) {
  profileGroup_->setEnabled(!running);
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
  const struct {
    QString name;
    QKeySequence sequence;
  } bindings[] = {
      {"开始/停止", startStopHotkeyEdit_->keySequence()},
      {"捕获坐标", captureHotkeyEdit_->keySequence()},
      {"紧急停止", emergencyHotkeyEdit_->keySequence()},
  };

  QStringList normalized;
  for (const auto& binding : bindings) {
    const QString text =
        binding.sequence.toString(QKeySequence::PortableText);
    if (text.isEmpty() || binding.sequence.count() != 1) {
      *errorMessage = QString("请为“%1”设置一个有效的单段热键。").arg(binding.name);
      return false;
    }
    if (normalized.contains(text)) {
      *errorMessage = QString("热键 %1 被重复使用。").arg(text);
      return false;
    }
    normalized.append(text);
  }

  return true;
}

QString MainWindow::selectedProfileName() const {
  return profileList_ && profileList_->currentItem()
             ? profileList_->currentItem()->text()
             : QString();
}
