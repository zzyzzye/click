#pragma once

#include <QObject>
#include <QString>

enum class AutomationActivity {
  Idle,
  Clicking,
  Recording,
  Playing
};
Q_DECLARE_METATYPE(AutomationActivity)

class AutomationCoordinator final : public QObject {
  Q_OBJECT

 public:
  explicit AutomationCoordinator(QObject* parent = nullptr);

  bool tryAcquire(AutomationActivity activity, QString* error = nullptr);
  void release(AutomationActivity activity);
  AutomationActivity activity() const;
  void requestEmergencyStop();

 signals:
  void activityChanged(AutomationActivity activity);
  void emergencyStopRequested();

 private:
  AutomationActivity activity_ = AutomationActivity::Idle;
};
