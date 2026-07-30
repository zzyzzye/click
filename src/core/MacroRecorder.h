#pragma once

#include <QObject>
#include <QStringList>

#include "core/MacroTypes.h"

struct MacroRecordingOptions {
  MacroTargetMode targetMode = MacroTargetMode::Global;
  WindowTarget target;
  QStringList reservedHotkeys;
};

class MacroRecorder : public QObject {
  Q_OBJECT

 public:
  explicit MacroRecorder(QObject* parent = nullptr) : QObject(parent) {}
  ~MacroRecorder() override = default;

  virtual bool start(const MacroRecordingOptions& options,
                     QString* error = nullptr) = 0;
  virtual void stop() = 0;
  virtual bool isRecording() const = 0;

 signals:
  void eventCaptured(const MacroEvent& event);
  void targetLost(const QString& reason);
  void recordingFailed(const QString& reason);
};
