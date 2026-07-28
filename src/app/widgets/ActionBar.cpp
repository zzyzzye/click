#include "app/widgets/ActionBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

ActionBar::ActionBar(QWidget* parent) : QFrame(parent) {
  setObjectName("actionBar");
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 12, 16, 12);
  summaryLabel_ = new QLabel(this);
  hintLabel_ = new QLabel("F8 紧急停止", this);
  startStopButton_ = new QPushButton("开始连点", this);
  startStopButton_->setObjectName("primaryAction");
  startStopButton_->setMinimumWidth(150);
  layout->addWidget(summaryLabel_, 1);
  layout->addWidget(hintLabel_);
  layout->addWidget(startStopButton_);
  connect(startStopButton_, &QPushButton::clicked, this,
          &ActionBar::startStopRequested);
}

void ActionBar::setRunning(bool running) {
  startStopButton_->setText(running ? "停止连点" : "开始连点");
  startStopButton_->setProperty("running", running);
}
void ActionBar::setSummary(const QString& summary) { summaryLabel_->setText(summary); }
QString ActionBar::buttonText() const { return startStopButton_->text(); }
QString ActionBar::summaryText() const { return summaryLabel_->text(); }
