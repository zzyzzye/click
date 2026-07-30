#pragma once

#include <QWidget>

#include "core/ClickTypes.h"

class QCheckBox;
class QComboBox;
class QPushButton;
class QSpinBox;
class QKeySequenceEdit;

class ClickSettingsPage final : public QWidget {
  Q_OBJECT

 public:
  explicit ClickSettingsPage(QWidget* parent = nullptr);

  void setProfile(const ClickProfile& profile);
  void applyToProfile(ClickProfile& profile) const;
  void setFixedPoint(const QPoint& point);
  void setEditingEnabled(bool enabled);
  bool fixedControlsEnabled() const;
  bool repeatCountEnabled() const;
  QString summary() const;

 signals:
  void captureRequested();
  void alwaysOnTopChanged(bool enabled);
  void settingsChanged();

 private:
  void updateDependencies();

  QSpinBox* interval_ = nullptr;
  QComboBox* inputMode_ = nullptr;
  QKeySequenceEdit* keyboardKey_ = nullptr;
  QComboBox* button_ = nullptr;
  QComboBox* targetMode_ = nullptr;
  QSpinBox* fixedX_ = nullptr;
  QSpinBox* fixedY_ = nullptr;
  QPushButton* capture_ = nullptr;
  QComboBox* repeatMode_ = nullptr;
  QSpinBox* repeatCount_ = nullptr;
  QSpinBox* jitter_ = nullptr;
  QSpinBox* countdown_ = nullptr;
  QCheckBox* alwaysOnTop_ = nullptr;
};
