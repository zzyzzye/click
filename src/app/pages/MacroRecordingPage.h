#pragma once

#include <QWidget>

#include "core/ClickTypes.h"
#include "core/MacroRecorder.h"
#include "core/MacroTypes.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class WindowPickerButton;

enum class MacroPageActivity {
  Idle,
  Recording,
  Playing,
  Unavailable
};
Q_DECLARE_METATYPE(MacroPageActivity)

class MacroRecordingPage final : public QWidget {
  Q_OBJECT

 public:
  explicit MacroRecordingPage(QWidget* parent = nullptr);

  void setHotkeys(const HotkeyBindings& hotkeys, bool registered);
  void setAvailableWindows(const QVector<WindowTarget>& windows,
                           quintptr selectedNativeId = 0);
  void setMacros(const QVector<MacroSequence>& macros,
                 const QString& selectedId = {});
  void setActivity(MacroPageActivity activity);
  void setSupported(bool supported, const QString& reason = {});
  void setRecordingProgress(qint64 durationUs, int eventCount);
  void setPlaybackProgress(int eventIndex, int eventCount);
  void setError(const QString& message);

  QString selectedMacroId() const;
  MacroPlaybackSettings playbackSettings() const;
  MacroRecordingOptions recordingOptions() const;

 signals:
  void recordRequested(const MacroRecordingOptions& options);
  void stopRequested();
  void playRequested(const QString& macroId,
                     const MacroPlaybackSettings& settings);
  void deleteRequested(const QString& macroId);
  void renameRequested(const QString& macroId);
  void refreshWindowsRequested();
  void windowPointSelected(const QPoint& globalPoint);

 private:
  void updateTargetControls();
  void updateRepeatControls();
  void updateMacroDetails();

  HotkeyBindings hotkeys_;
  MacroPageActivity activity_ = MacroPageActivity::Idle;
  QLabel* recordHotkeyLabel_ = nullptr;
  QLabel* playbackHotkeyLabel_ = nullptr;
  QLabel* emergencyHotkeyLabel_ = nullptr;
  QLabel* hotkeyStatusLabel_ = nullptr;
  QComboBox* targetMode_ = nullptr;
  QComboBox* window_ = nullptr;
  QPushButton* refreshWindows_ = nullptr;
  WindowPickerButton* windowPicker_ = nullptr;
  QComboBox* macros_ = nullptr;
  QLabel* macroDetails_ = nullptr;
  QLabel* errorLabel_ = nullptr;
  QPushButton* rename_ = nullptr;
  QPushButton* delete_ = nullptr;
  QComboBox* speed_ = nullptr;
  QComboBox* repeatMode_ = nullptr;
  QSpinBox* repeatCount_ = nullptr;
  QSpinBox* loopDelay_ = nullptr;
  QPushButton* record_ = nullptr;
  QPushButton* play_ = nullptr;
  bool supported_ = true;
};
