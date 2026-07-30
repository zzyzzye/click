#include "app/pages/HotkeySettingsPage.h"
#include <QFrame>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

HotkeySettingsPage::HotkeySettingsPage(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSizeConstraint(QLayout::SetMinimumSize);
  auto* card = new QFrame(this); card->setObjectName("settingsCard");
  auto* layout = new QFormLayout(card);
  auto* title = new QLabel("全局热键", card); title->setObjectName("cardTitle");
  layout->addRow(title);
  startStop_ = new QKeySequenceEdit(card); capture_ = new QKeySequenceEdit(card);
  emergency_ = new QKeySequenceEdit(card);
  macroRecord_ = new QKeySequenceEdit(card);
  macroRecord_->setObjectName("macroRecordHotkeyEdit");
  macroPlayback_ = new QKeySequenceEdit(card);
  macroPlayback_->setObjectName("macroPlaybackHotkeyEdit");
  layout->addRow("开始 / 停止", startStop_);
  layout->addRow("捕获坐标", capture_);
  layout->addRow("紧急停止", emergency_);
  layout->addRow("宏录制 / 停止", macroRecord_);
  layout->addRow("宏回放 / 停止", macroPlayback_);
  layout->addRow(new QLabel("热键被其他应用占用时，ClickFlow 会拒绝整组注册。", card));
  root->addWidget(card); root->addStretch();
  for (auto* editor : {startStop_, capture_, emergency_, macroRecord_, macroPlayback_}) {
    connect(editor, &QKeySequenceEdit::keySequenceChanged, this,
            [this] { emit hotkeysChanged(); });
  }
}
void HotkeySettingsPage::setProfile(const ClickProfile& p) {
  const QSignalBlocker blockStart(startStop_);
  const QSignalBlocker blockCapture(capture_);
  const QSignalBlocker blockEmergency(emergency_);
  const QSignalBlocker blockRecord(macroRecord_);
  const QSignalBlocker blockPlayback(macroPlayback_);
  startStop_->setKeySequence(QKeySequence::fromString(p.hotkeys.startStop, QKeySequence::PortableText));
  capture_->setKeySequence(QKeySequence::fromString(p.hotkeys.capturePoint, QKeySequence::PortableText));
  emergency_->setKeySequence(QKeySequence::fromString(p.hotkeys.emergencyStop, QKeySequence::PortableText));
  macroRecord_->setKeySequence(QKeySequence::fromString(
      p.hotkeys.macroRecord, QKeySequence::PortableText));
  macroPlayback_->setKeySequence(QKeySequence::fromString(
      p.hotkeys.macroPlayback, QKeySequence::PortableText));
}
void HotkeySettingsPage::applyToProfile(ClickProfile& p) const {
  p.hotkeys.startStop = startStop_->keySequence().toString(QKeySequence::PortableText);
  p.hotkeys.capturePoint = capture_->keySequence().toString(QKeySequence::PortableText);
  p.hotkeys.emergencyStop = emergency_->keySequence().toString(QKeySequence::PortableText);
  p.hotkeys.macroRecord =
      macroRecord_->keySequence().toString(QKeySequence::PortableText);
  p.hotkeys.macroPlayback =
      macroPlayback_->keySequence().toString(QKeySequence::PortableText);
}
bool HotkeySettingsPage::validate(const ClickProfile& profile, QString* error) const {
  const struct { QString name; QKeySequence value; } values[] = {
    {"开始/停止", startStop_->keySequence()}, {"捕获坐标", capture_->keySequence()},
    {"紧急停止", emergency_->keySequence()},
    {"宏录制/停止", macroRecord_->keySequence()},
    {"宏回放/停止", macroPlayback_->keySequence()}};
  QStringList seen;
  for (const auto& value : values) {
    const QString text = value.value.toString(QKeySequence::PortableText);
    if (text.isEmpty() || value.value.count() != 1) {
      *error = QString("请为“%1”设置有效的单段热键。").arg(value.name); return false;
    }
    if (seen.contains(text)) { *error = QString("热键 %1 被重复使用。").arg(text); return false; }
    seen.append(text);
  }
  if (profile.inputMode == InputMode::Keyboard) {
    const QKeySequence repeated = QKeySequence::fromString(profile.keyboardKey, QKeySequence::PortableText);
    if (repeated.count() != 1) { *error = "请选择一个有效的键盘按键。"; return false; }
    const int repeatedKey = repeated[0].toCombined() & ~Qt::KeyboardModifierMask;
    for (const auto& value : values) {
      if ((value.value[0].toCombined() & ~Qt::KeyboardModifierMask) == repeatedKey) {
        *error = QString("连按键 %1 与“%2”热键冲突，请更换该热键后再开始。").arg(profile.keyboardKey, value.name); return false;
      }
    }
  }
  return true;
}
void HotkeySettingsPage::setEditingEnabled(bool enabled) { setEnabled(enabled); }
