#include "app/widgets/ActionBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

ActionBar::ActionBar(QWidget* parent) : QFrame(parent) {
  setObjectName("actionBar");
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 12, 16, 12);
  summaryLabel_ = new QLabel(this);
  hintLabel_ = new QLabel(this);
  hintLabel_->setObjectName("actionHint");
  hintLabel_->setWordWrap(true);
  summaryLabel_->setWordWrap(true);
  startStopButton_ = new QPushButton("开始连点", this);
  startStopButton_->setObjectName("startStopButton");
  startStopButton_->setMinimumWidth(150);
  layout->addWidget(summaryLabel_, 1);
  layout->addWidget(hintLabel_);
  layout->addWidget(startStopButton_);
  connect(startStopButton_, &QPushButton::clicked, this,
          &ActionBar::startStopRequested);
}

void ActionBar::setHotkeys(const HotkeyBindings& hotkeys) {
  hintLabel_->setText(QString("启动：%1　停止：%2").arg(hotkeys.startStop, hotkeys.emergencyStop));
  hintLabel_->setToolTip("也可以在“热键”页面修改快捷键。紧急停止会立即停止连点和宏回放。");
}

void ActionBar::setRunning(bool running) {
  startStopButton_->setText(running ? "停止连点" : "开始连点");
  startStopButton_->setProperty("running", running);
  // 动态属性变化后主动刷新 QSS，确保停止按钮立即呈现危险色。
  startStopButton_->style()->unpolish(startStopButton_);
  startStopButton_->style()->polish(startStopButton_);
  startStopButton_->update();
}
void ActionBar::setSummary(const QString& summary) { summaryLabel_->setText(summary); }
QString ActionBar::buttonText() const { return startStopButton_->text(); }
QString ActionBar::summaryText() const { return summaryLabel_->text(); }
