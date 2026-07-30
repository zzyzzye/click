#pragma once
#include <QWidget>
#include "core/ClickTypes.h"
class QKeySequenceEdit;
class HotkeySettingsPage final : public QWidget {
  Q_OBJECT
 public:
  explicit HotkeySettingsPage(QWidget* parent = nullptr);
  void setProfile(const ClickProfile& profile);
  void applyToProfile(ClickProfile& profile) const;
  bool validate(const ClickProfile& profile, QString* error) const;
  void setEditingEnabled(bool enabled);
 private:
  QKeySequenceEdit* startStop_ = nullptr;
  QKeySequenceEdit* capture_ = nullptr;
  QKeySequenceEdit* emergency_ = nullptr;
  QKeySequenceEdit* macroRecord_ = nullptr;
  QKeySequenceEdit* macroPlayback_ = nullptr;
};
