#include "app/pages/MacroRecordingPage.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

#include "app/widgets/WindowPickerButton.h"

namespace {

QFrame* card(QWidget* parent, const QString& title, QVBoxLayout** content) {
  auto* frame = new QFrame(parent);
  frame->setObjectName("settingsCard");
  auto* layout = new QVBoxLayout(frame);
  layout->setContentsMargins(18, 16, 18, 16);
  layout->setSpacing(12);
  auto* heading = new QLabel(title, frame);
  heading->setObjectName("cardTitle");
  layout->addWidget(heading);
  *content = layout;
  return frame;
}

QString durationText(qint64 durationUs) {
  const qint64 totalMs = std::max<qint64>(0, durationUs / 1000);
  const qint64 minutes = totalMs / 60000;
  const qint64 seconds = (totalMs / 1000) % 60;
  const qint64 milliseconds = totalMs % 1000;
  return QString("%1:%2.%3")
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(seconds, 2, 10, QLatin1Char('0'))
      .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

}  // namespace

MacroRecordingPage::MacroRecordingPage(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(12);
  root->setSizeConstraint(QLayout::SetMinimumSize);

  QVBoxLayout* hotkeyContent = nullptr;
  auto* hotkeyCard = card(this, "控制热键", &hotkeyContent);
  auto* hotkeyRow = new QHBoxLayout;
  recordHotkeyLabel_ = new QLabel(hotkeyCard);
  recordHotkeyLabel_->setObjectName("macroRecordHotkeyLabel");
  recordHotkeyLabel_->setProperty("hotkeyChip", true);
  playbackHotkeyLabel_ = new QLabel(hotkeyCard);
  playbackHotkeyLabel_->setObjectName("macroPlaybackHotkeyLabel");
  playbackHotkeyLabel_->setProperty("hotkeyChip", true);
  emergencyHotkeyLabel_ = new QLabel(hotkeyCard);
  emergencyHotkeyLabel_->setObjectName("macroEmergencyHotkeyLabel");
  emergencyHotkeyLabel_->setProperty("emergencyChip", true);
  hotkeyRow->addWidget(recordHotkeyLabel_);
  hotkeyRow->addWidget(playbackHotkeyLabel_);
  hotkeyRow->addWidget(emergencyHotkeyLabel_);
  hotkeyRow->addStretch();
  hotkeyContent->addLayout(hotkeyRow);
  hotkeyStatusLabel_ = new QLabel(hotkeyCard);
  hotkeyStatusLabel_->setObjectName("macroHotkeyRegistrationStatus");
  hotkeyContent->addWidget(hotkeyStatusLabel_);
  root->addWidget(hotkeyCard);

  QVBoxLayout* targetContent = nullptr;
  auto* targetCard = card(this, "录制目标", &targetContent);
  auto* targetForm = new QFormLayout;
  targetForm->setContentsMargins(0, 0, 0, 0);
  targetMode_ = new QComboBox(targetCard);
  targetMode_->setObjectName("macroTargetModeCombo");
  targetMode_->addItem("整个系统（跨应用）",
                       static_cast<int>(MacroTargetMode::Global));
  targetMode_->addItem("绑定一个窗口",
                       static_cast<int>(MacroTargetMode::Window));
  targetForm->addRow("范围", targetMode_);
  auto* windowRow = new QWidget(targetCard);
  auto* windowLayout = new QHBoxLayout(windowRow);
  windowLayout->setContentsMargins(0, 0, 0, 0);
  window_ = new QComboBox(windowRow);
  window_->setObjectName("macroWindowCombo");
  window_->setMinimumContentsLength(24);
  refreshWindows_ = new QPushButton("刷新", windowRow);
  refreshWindows_->setObjectName("macroRefreshWindowsButton");
  windowPicker_ = new WindowPickerButton(windowRow);
  windowLayout->addWidget(window_, 1);
  windowLayout->addWidget(refreshWindows_);
  windowLayout->addWidget(windowPicker_);
  targetForm->addRow("目标窗口", windowRow);
  targetContent->addLayout(targetForm);
  root->addWidget(targetCard);

  QVBoxLayout* libraryContent = nullptr;
  auto* libraryCard = card(this, "本机录制", &libraryContent);
  auto* libraryRow = new QHBoxLayout;
  macros_ = new QComboBox(libraryCard);
  macros_->setObjectName("macroLibraryCombo");
  rename_ = new QPushButton("重命名", libraryCard);
  rename_->setObjectName("macroRenameButton");
  delete_ = new QPushButton("删除", libraryCard);
  delete_->setObjectName("macroDeleteButton");
  libraryRow->addWidget(macros_, 1);
  libraryRow->addWidget(rename_);
  libraryRow->addWidget(delete_);
  libraryContent->addLayout(libraryRow);
  macroDetails_ = new QLabel("尚未保存录制", libraryCard);
  macroDetails_->setObjectName("macroDetailsLabel");
  libraryContent->addWidget(macroDetails_);
  root->addWidget(libraryCard);

  QVBoxLayout* playbackContent = nullptr;
  auto* playbackCard = card(this, "回放设置", &playbackContent);
  auto* playbackForm = new QFormLayout;
  playbackForm->setContentsMargins(0, 0, 0, 0);
  speed_ = new QComboBox(playbackCard);
  speed_->setObjectName("macroSpeedCombo");
  for (double value : {0.5, 1.0, 1.5, 2.0}) {
    speed_->addItem(QString::number(value, 'g', 2) + "×", value);
  }
  speed_->setCurrentIndex(speed_->findData(1.0));
  repeatMode_ = new QComboBox(playbackCard);
  repeatMode_->setObjectName("macroRepeatModeCombo");
  repeatMode_->addItem("指定次数", false);
  repeatMode_->addItem("无限循环", true);
  repeatCount_ = new QSpinBox(playbackCard);
  repeatCount_->setObjectName("macroRepeatCountSpin");
  repeatCount_->setRange(1, 999999);
  loopDelay_ = new QSpinBox(playbackCard);
  loopDelay_->setObjectName("macroLoopDelaySpin");
  loopDelay_->setRange(0, 3600000);
  loopDelay_->setSuffix(" 毫秒");
  playbackForm->addRow("速度", speed_);
  playbackForm->addRow("循环", repeatMode_);
  playbackForm->addRow("次数", repeatCount_);
  playbackForm->addRow("每轮等待", loopDelay_);
  playbackContent->addLayout(playbackForm);
  root->addWidget(playbackCard);

  auto* actionCard = new QFrame(this);
  actionCard->setObjectName("settingsCard");
  auto* actions = new QHBoxLayout(actionCard);
  actions->setContentsMargins(18, 14, 18, 14);
  errorLabel_ = new QLabel(actionCard);
  errorLabel_->setObjectName("macroErrorLabel");
  errorLabel_->setWordWrap(true);
  record_ = new QPushButton("开始录制", actionCard);
  record_->setObjectName("macroRecordButton");
  play_ = new QPushButton("开始回放", actionCard);
  play_->setObjectName("macroPlayButton");
  actions->addWidget(errorLabel_, 1);
  actions->addWidget(record_);
  actions->addWidget(play_);
  root->addWidget(actionCard);
  root->addStretch();

  setStyleSheet(R"(
    QLabel[hotkeyChip="true"] {
      background: #e8f0ff; color: #194ca8; border: 1px solid #b9cef7;
      border-radius: 7px; padding: 7px 11px; font-family: Consolas;
      font-weight: 600;
    }
    QLabel[emergencyChip="true"] {
      background: #fff0f0; color: #b42318; border: 1px solid #f1b8b4;
      border-radius: 7px; padding: 7px 11px; font-family: Consolas;
      font-weight: 600;
    }
    #macroErrorLabel { color: #b42318; }
    #macroRecordButton { background: #2563eb; color: white; font-weight: 650;
      border: 0; border-radius: 8px; padding: 9px 18px; }
    #macroPlayButton { background: #173b66; color: white; font-weight: 650;
      border: 0; border-radius: 8px; padding: 9px 18px; }
  )");

  connect(targetMode_, &QComboBox::currentIndexChanged, this,
          &MacroRecordingPage::updateTargetControls);
  connect(repeatMode_, &QComboBox::currentIndexChanged, this,
          &MacroRecordingPage::updateRepeatControls);
  connect(macros_, &QComboBox::currentIndexChanged, this,
          &MacroRecordingPage::updateMacroDetails);
  connect(refreshWindows_, &QPushButton::clicked, this,
          &MacroRecordingPage::refreshWindowsRequested);
  connect(windowPicker_, &WindowPickerButton::windowPointSelected, this,
          &MacroRecordingPage::windowPointSelected);
  connect(rename_, &QPushButton::clicked, this, [this] {
    if (!selectedMacroId().isEmpty()) emit renameRequested(selectedMacroId());
  });
  connect(delete_, &QPushButton::clicked, this, [this] {
    if (!selectedMacroId().isEmpty()) emit deleteRequested(selectedMacroId());
  });
  connect(record_, &QPushButton::clicked, this, [this] {
    if (activity_ == MacroPageActivity::Recording) emit stopRequested();
    else if (activity_ == MacroPageActivity::Idle) emit recordRequested(recordingOptions());
  });
  connect(play_, &QPushButton::clicked, this, [this] {
    if (activity_ == MacroPageActivity::Playing) emit stopRequested();
    else if (activity_ == MacroPageActivity::Idle && !selectedMacroId().isEmpty()) {
      emit playRequested(selectedMacroId(), playbackSettings());
    }
  });

  setHotkeys(HotkeyBindings{}, true);
  updateTargetControls();
  updateRepeatControls();
  updateMacroDetails();
  setActivity(MacroPageActivity::Idle);
}

void MacroRecordingPage::setHotkeys(const HotkeyBindings& hotkeys,
                                    bool registered) {
  hotkeys_ = hotkeys;
  recordHotkeyLabel_->setText(QString("录制  %1").arg(hotkeys.macroRecord));
  playbackHotkeyLabel_->setText(QString("回放  %1").arg(hotkeys.macroPlayback));
  emergencyHotkeyLabel_->setText(QString("紧急停止  %1").arg(hotkeys.emergencyStop));
  hotkeyStatusLabel_->setText(registered ? "全局热键已就绪"
                                         : "全局热键未就绪，请前往“热键”页面检查冲突");
  hotkeyStatusLabel_->setStyleSheet(registered ? "color: #247a4d;"
                                               : "color: #b42318;");
}

void MacroRecordingPage::setAvailableWindows(
    const QVector<WindowTarget>& windows, quintptr selectedNativeId) {
  window_->clear();
  for (const auto& target : windows) {
    const QString appName = QFileInfo(target.executablePath).fileName();
    window_->addItem(QString("%1 — %2").arg(target.title, appName),
                     QVariant::fromValue(target));
    if (target.nativeId == selectedNativeId) window_->setCurrentIndex(window_->count() - 1);
  }
  updateTargetControls();
}

void MacroRecordingPage::setMacros(const QVector<MacroSequence>& macros,
                                   const QString& selectedId) {
  macros_->clear();
  for (const auto& sequence : macros) {
    macros_->addItem(sequence.name, sequence.id);
    macros_->setItemData(macros_->count() - 1, QVariant::fromValue(sequence),
                         Qt::UserRole + 1);
    if (sequence.id == selectedId) macros_->setCurrentIndex(macros_->count() - 1);
  }
  updateMacroDetails();
  setActivity(activity_);
}

void MacroRecordingPage::setActivity(MacroPageActivity activity) {
  activity_ = activity;
  const bool idle = activity == MacroPageActivity::Idle && supported_;
  const bool hasMacro = !selectedMacroId().isEmpty();
  targetMode_->setEnabled(idle);
  window_->setEnabled(idle && targetMode_->currentData().toInt() ==
                                  static_cast<int>(MacroTargetMode::Window));
  refreshWindows_->setEnabled(window_->isEnabled());
  windowPicker_->setEnabled(window_->isEnabled());
  macros_->setEnabled(idle);
  rename_->setEnabled(idle && hasMacro);
  delete_->setEnabled(idle && hasMacro);
  speed_->setEnabled(idle);
  repeatMode_->setEnabled(idle);
  repeatCount_->setEnabled(idle && !repeatMode_->currentData().toBool());
  loopDelay_->setEnabled(idle);

  record_->setEnabled(supported_ &&
                      (idle || activity == MacroPageActivity::Recording));
  play_->setEnabled(supported_ &&
                    ((idle && hasMacro) || activity == MacroPageActivity::Playing));
  record_->setText(activity == MacroPageActivity::Recording ? "停止录制" : "开始录制");
  play_->setText(activity == MacroPageActivity::Playing ? "停止回放" : "开始回放");
}

void MacroRecordingPage::setSupported(bool supported, const QString& reason) {
  supported_ = supported;
  if (!supported && !reason.isEmpty()) setError(reason);
  setActivity(supported ? MacroPageActivity::Idle : MacroPageActivity::Unavailable);
}

void MacroRecordingPage::setRecordingProgress(qint64 durationUs, int eventCount) {
  macroDetails_->setText(QString("录制中 · %1 · %2 个事件")
                             .arg(durationText(durationUs))
                             .arg(eventCount));
}

void MacroRecordingPage::setPlaybackProgress(int eventIndex, int eventCount) {
  macroDetails_->setText(QString("回放进度 · %1 / %2 个事件")
                             .arg(eventIndex)
                             .arg(eventCount));
}

void MacroRecordingPage::setError(const QString& message) {
  errorLabel_->setText(message);
}

QString MacroRecordingPage::selectedMacroId() const {
  return macros_->currentData().toString();
}

MacroPlaybackSettings MacroRecordingPage::playbackSettings() const {
  MacroPlaybackSettings settings;
  settings.speed = speed_->currentData().toDouble();
  settings.infinite = repeatMode_->currentData().toBool();
  settings.repeatCount = repeatCount_->value();
  settings.loopDelayMs = loopDelay_->value();
  return settings;
}

MacroRecordingOptions MacroRecordingPage::recordingOptions() const {
  MacroRecordingOptions options;
  options.targetMode =
      static_cast<MacroTargetMode>(targetMode_->currentData().toInt());
  if (options.targetMode == MacroTargetMode::Window && window_->currentIndex() >= 0) {
    options.target = window_->currentData().value<WindowTarget>();
  }
  options.reservedHotkeys =
      {hotkeys_.macroRecord, hotkeys_.macroPlayback, hotkeys_.emergencyStop};
  return options;
}

void MacroRecordingPage::updateTargetControls() {
  const bool windowMode = targetMode_->currentData().toInt() ==
                          static_cast<int>(MacroTargetMode::Window);
  const bool enabled = supported_ && activity_ == MacroPageActivity::Idle && windowMode;
  window_->setEnabled(enabled);
  refreshWindows_->setEnabled(enabled);
  windowPicker_->setEnabled(enabled);
}

void MacroRecordingPage::updateRepeatControls() {
  repeatCount_->setEnabled(supported_ && activity_ == MacroPageActivity::Idle &&
                           !repeatMode_->currentData().toBool());
}

void MacroRecordingPage::updateMacroDetails() {
  const auto sequence = macros_->currentData(Qt::UserRole + 1).value<MacroSequence>();
  if (sequence.id.isEmpty()) {
    macroDetails_->setText("尚未保存录制");
  } else {
    const QString target = sequence.targetMode == MacroTargetMode::Window
                               ? QString("窗口：%1").arg(sequence.target.title)
                               : "整个系统";
    macroDetails_->setText(QString("%1 · %2 个事件 · %3")
                               .arg(durationText(sequence.durationUs))
                               .arg(sequence.events.size())
                               .arg(target));
  }
  const bool idle = activity_ == MacroPageActivity::Idle && supported_;
  rename_->setEnabled(idle && !sequence.id.isEmpty());
  delete_->setEnabled(idle && !sequence.id.isEmpty());
  play_->setEnabled(idle && !sequence.id.isEmpty());
}
