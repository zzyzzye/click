#pragma once

#include <QFrame>

class QLabel;

class StatusStrip final : public QFrame {
  Q_OBJECT

 public:
  explicit StatusStrip(QWidget* parent = nullptr);

  void setPermissionState(bool available);
  void setStatus(const QString& status);
  void setProgress(const QString& progress);
  QString permissionText() const;
  QString statusText() const;
  QString progressText() const;

 private:
  QLabel* permissionLabel_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLabel* progressLabel_ = nullptr;
};
