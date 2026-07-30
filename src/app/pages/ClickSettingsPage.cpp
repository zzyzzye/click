#include "app/pages/ClickSettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QFrame* card(const QString& title, QWidget* parent, QGridLayout** grid) {
  auto* frame = new QFrame(parent);
  frame->setObjectName("settingsCard");
  auto* outer = new QVBoxLayout(frame);
  auto* heading = new QLabel(title, frame);
  heading->setObjectName("cardTitle");
  outer->addWidget(heading);
  *grid = new QGridLayout();
  (*grid)->setHorizontalSpacing(16);
  (*grid)->setVerticalSpacing(12);
  (*grid)->setColumnStretch(1, 1);
  outer->addLayout(*grid);
  return frame;
}
void addRow(QGridLayout* grid, int row, const QString& label, QWidget* control) {
  grid->addWidget(new QLabel(label), row, 0);
  grid->addWidget(control, row, 1);
}
}

ClickSettingsPage::ClickSettingsPage(QWidget* parent) : QWidget(parent) {
  setObjectName("clickSettingsPage");
  auto* root = new QVBoxLayout(this);
  root->setSizeConstraint(QLayout::SetMinimumSize);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(16);

  QGridLayout* basic = nullptr;
  auto* basicCard = card("基础设置", this, &basic);
  interval_ = new QSpinBox(this);
  interval_->setRange(1, 600000); interval_->setSuffix(" 毫秒");
  inputMode_ = new QComboBox(this);
  inputMode_->addItem("鼠标", int(InputMode::Mouse));
  inputMode_->addItem("键盘", int(InputMode::Keyboard));
  keyboardKey_ = new QKeySequenceEdit(this);
  keyboardKey_->setMaximumSequenceLength(1);
  keyboardKey_->setKeySequence(QKeySequence(Qt::Key_Space));
  button_ = new QComboBox(this);
  button_->addItem("左键", int(ClickButton::Left));
  button_->addItem("右键", int(ClickButton::Right));
  targetMode_ = new QComboBox(this);
  targetMode_->setObjectName("targetModeCombo");
  targetMode_->addItem("跟随鼠标", int(TargetMode::FollowCursor));
  targetMode_->addItem("固定坐标", int(TargetMode::FixedPoint));
  auto* coordinates = new QWidget(this);
  auto* coordinateLayout = new QHBoxLayout(coordinates);
  coordinateLayout->setContentsMargins(0, 0, 0, 0);
  fixedX_ = new QSpinBox(this); fixedX_->setRange(-100000, 100000); fixedX_->setPrefix("X ");
  fixedX_->setObjectName("fixedXSpin");
  fixedY_ = new QSpinBox(this); fixedY_->setRange(-100000, 100000); fixedY_->setPrefix("Y ");
  fixedY_->setObjectName("fixedYSpin");
  capture_ = new QPushButton("捕获当前位置", this);
  coordinateLayout->addWidget(fixedX_); coordinateLayout->addWidget(fixedY_);
  coordinateLayout->addWidget(capture_);
  addRow(basic, 0, "点击间隔", interval_);
  addRow(basic, 1, "连点类型", inputMode_);
  addRow(basic, 2, "键盘按键", keyboardKey_);
  addRow(basic, 3, "鼠标按键", button_);
  addRow(basic, 4, "点击位置", targetMode_);
  addRow(basic, 5, "固定坐标", coordinates);
  root->addWidget(basicCard);

  QGridLayout* behavior = nullptr;
  auto* behaviorCard = card("运行行为", this, &behavior);
  repeatMode_ = new QComboBox(this);
  repeatMode_->setObjectName("repeatModeCombo");
  repeatMode_->addItem("无限", int(RepeatMode::Infinite));
  repeatMode_->addItem("有限次数", int(RepeatMode::Finite));
  repeatCount_ = new QSpinBox(this); repeatCount_->setRange(1, 100000000);
  repeatCount_->setObjectName("repeatCountSpin");
  jitter_ = new QSpinBox(this); jitter_->setRange(0, 1000); jitter_->setSuffix(" 像素");
  countdown_ = new QSpinBox(this); countdown_->setRange(0, 3600); countdown_->setSuffix(" 秒");
  alwaysOnTop_ = new QCheckBox("保持窗口置顶", this);
  addRow(behavior, 0, "重复方式", repeatMode_);
  addRow(behavior, 1, "点击次数", repeatCount_);
  addRow(behavior, 2, "抖动半径", jitter_);
  addRow(behavior, 3, "开始倒计时", countdown_);
  addRow(behavior, 4, "窗口", alwaysOnTop_);
  root->addWidget(behaviorCard);
  root->addStretch();

  connect(targetMode_, &QComboBox::currentIndexChanged, this, &ClickSettingsPage::updateDependencies);
  connect(inputMode_, &QComboBox::currentIndexChanged, this, &ClickSettingsPage::updateDependencies);
  connect(repeatMode_, &QComboBox::currentIndexChanged, this, &ClickSettingsPage::updateDependencies);
  connect(capture_, &QPushButton::clicked, this, &ClickSettingsPage::captureRequested);
  connect(alwaysOnTop_, &QCheckBox::toggled, this, &ClickSettingsPage::alwaysOnTopChanged);
  for (auto* spin : {interval_, fixedX_, fixedY_, repeatCount_, jitter_, countdown_})
    connect(spin, &QSpinBox::valueChanged, this, &ClickSettingsPage::settingsChanged);
  connect(button_, &QComboBox::currentIndexChanged, this, &ClickSettingsPage::settingsChanged);
  connect(inputMode_, &QComboBox::currentIndexChanged, this, &ClickSettingsPage::settingsChanged);
  connect(keyboardKey_, &QKeySequenceEdit::keySequenceChanged, this, &ClickSettingsPage::settingsChanged);
  connect(targetMode_, &QComboBox::currentIndexChanged, this, &ClickSettingsPage::settingsChanged);
  connect(repeatMode_, &QComboBox::currentIndexChanged, this, &ClickSettingsPage::settingsChanged);
  updateDependencies();
}

void ClickSettingsPage::setProfile(const ClickProfile& p) {
  interval_->setValue(p.intervalMs);
  inputMode_->setCurrentIndex(inputMode_->findData(int(p.inputMode)));
  keyboardKey_->setKeySequence(QKeySequence::fromString(p.keyboardKey, QKeySequence::PortableText));
  button_->setCurrentIndex(button_->findData(int(p.button)));
  targetMode_->setCurrentIndex(targetMode_->findData(int(p.targetMode)));
  fixedX_->setValue(p.fixedPoint.x()); fixedY_->setValue(p.fixedPoint.y());
  repeatMode_->setCurrentIndex(repeatMode_->findData(int(p.repeatMode)));
  repeatCount_->setValue(p.repeatCount); jitter_->setValue(p.jitterRadius);
  countdown_->setValue(p.countdownSeconds); alwaysOnTop_->setChecked(p.alwaysOnTop);
  updateDependencies();
}
void ClickSettingsPage::applyToProfile(ClickProfile& p) const {
  p.intervalMs = interval_->value(); p.button = ClickButton(button_->currentData().toInt());
  p.inputMode = InputMode(inputMode_->currentData().toInt());
  p.keyboardKey = keyboardKey_->keySequence().toString(QKeySequence::PortableText);
  p.targetMode = TargetMode(targetMode_->currentData().toInt());
  p.fixedPoint = QPoint(fixedX_->value(), fixedY_->value());
  p.repeatMode = RepeatMode(repeatMode_->currentData().toInt());
  p.repeatCount = repeatCount_->value(); p.jitterRadius = jitter_->value();
  p.countdownSeconds = countdown_->value(); p.alwaysOnTop = alwaysOnTop_->isChecked();
}
void ClickSettingsPage::setFixedPoint(const QPoint& point) {
  fixedX_->setValue(point.x()); fixedY_->setValue(point.y());
  targetMode_->setCurrentIndex(targetMode_->findData(int(TargetMode::FixedPoint)));
}
void ClickSettingsPage::setEditingEnabled(bool enabled) { setEnabled(enabled); }
bool ClickSettingsPage::fixedControlsEnabled() const { return fixedX_->isEnabled(); }
bool ClickSettingsPage::repeatCountEnabled() const { return repeatCount_->isEnabled(); }
QString ClickSettingsPage::summary() const {
  const bool keyboard = inputMode_->currentData().toInt() == int(InputMode::Keyboard);
  return QString("%1 毫秒 · %2 · %3")
      .arg(interval_->value())
      .arg(keyboard ? QString("键盘 %1").arg(keyboardKey_->keySequence().toString(QKeySequence::PortableText)) : (targetMode_->currentData().toInt() == int(TargetMode::FollowCursor) ? "跟随鼠标" : "固定坐标"))
      .arg(repeatMode_->currentData().toInt() == int(RepeatMode::Infinite) ? "无限" : QString("%1 次").arg(repeatCount_->value()));
}
void ClickSettingsPage::updateDependencies() {
  const bool keyboard = inputMode_->currentData().toInt() == int(InputMode::Keyboard);
  keyboardKey_->setEnabled(keyboard);
  button_->setEnabled(!keyboard); targetMode_->setEnabled(!keyboard);
  const bool fixed = targetMode_->currentData().toInt() == int(TargetMode::FixedPoint);
  fixedX_->setEnabled(!keyboard && fixed); fixedY_->setEnabled(!keyboard && fixed); capture_->setEnabled(!keyboard && fixed);
  repeatCount_->setEnabled(repeatMode_->currentData().toInt() == int(RepeatMode::Finite));
}
