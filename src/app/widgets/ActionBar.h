#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

class ActionBar final : public QFrame {
  Q_OBJECT

 public:
  explicit ActionBar(QWidget* parent = nullptr);

  void setRunning(bool running);
  void setSummary(const QString& summary);
  QString buttonText() const;
  QString summaryText() const;

 signals:
  void startStopRequested();

 private:
  QLabel* summaryLabel_ = nullptr;
  QLabel* hintLabel_ = nullptr;
  QPushButton* startStopButton_ = nullptr;
};
