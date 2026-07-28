#include "app/widgets/StatusStrip.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

StatusStrip::StatusStrip(QWidget* parent) : QFrame(parent) {
  setObjectName("statusStrip");
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 10, 16, 10);
  permissionLabel_ = new QLabel(this);
  statusLabel_ = new QLabel("空闲", this);
  progressLabel_ = new QLabel("就绪", this);
  layout->addWidget(permissionLabel_);
  layout->addStretch();
  layout->addWidget(statusLabel_);
  layout->addWidget(progressLabel_);
  setPermissionState(true);
}

void StatusStrip::setPermissionState(bool available) {
  permissionLabel_->setText(available ? "输入控制可用" : "需要输入控制权限");
  setProperty("permissionAvailable", available);
  style()->unpolish(this);
  style()->polish(this);
}
void StatusStrip::setStatus(const QString& status) { statusLabel_->setText(status); }
void StatusStrip::setProgress(const QString& progress) { progressLabel_->setText(progress); }
QString StatusStrip::permissionText() const { return permissionLabel_->text(); }
QString StatusStrip::statusText() const { return statusLabel_->text(); }
QString StatusStrip::progressText() const { return progressLabel_->text(); }
