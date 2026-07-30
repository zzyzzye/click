#pragma once
#include <QWidget>
#include "core/ClickTypes.h"
class QCheckBox;
class QKeySequenceEdit;
class QLabel;
class HotkeySettingsPage final : public QWidget {
  Q_OBJECT
 public:
  explicit HotkeySettingsPage(QWidget* parent = nullptr);
  void setProfile(const ClickProfile& profile);
  void applyToProfile(ClickProfile& profile) const;
  bool validate(const ClickProfile& profile, QString* error) const;
  void setEditingEnabled(bool enabled);
  void setActivationState(bool enabled, const QString& status);
  bool activationEnabled() const;
 signals:
  void hotkeysChanged();
  void activationRequested(bool enabled);
 private:
  QCheckBox* activation_ = nullptr;
  QLabel* activationStatus_ = nullptr;
  QKeySequenceEdit* startStop_ = nullptr;
  QKeySequenceEdit* capture_ = nullptr;
  QKeySequenceEdit* emergency_ = nullptr;
  QKeySequenceEdit* macroRecord_ = nullptr;
  QKeySequenceEdit* macroPlayback_ = nullptr;
};
