#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include "core/AutomationCoordinator.h"
#include "core/MacroPlayer.h"
#include "core/MacroRecorder.h"
#include "core/MacroTypes.h"

enum class MacroControllerState { Idle, Recording, Playing, Paused };
Q_DECLARE_METATYPE(MacroControllerState)

class MacroController final : public QObject {
  Q_OBJECT

 public:
  MacroController(MacroRecorder* recorder, MacroPlayer* player,
                  AutomationCoordinator* coordinator, QObject* parent = nullptr);
  ~MacroController() override;

  bool startRecording(const MacroRecordingOptions& options,
                      QString* error = nullptr);
  void stopRecording();
  bool startPlayback(const MacroSequence& sequence, QString* error = nullptr);
  void stop();
  void emergencyStop();
  void pause();
  bool resume(QString* error = nullptr);
  bool isPaused() const {
    return state_ == MacroControllerState::Paused;
  }

  MacroControllerState state() const;
  bool isRecording() const;
  bool isPlaying() const;
  const MacroSequence& currentSequence() const;

  static qint64 scaledOffsetUs(qint64 recordedOffsetUs, double speed);

 signals:
  void stateChanged(MacroControllerState state);
  void recordingProgress(qint64 durationUs, int eventCount);
  void recordingCompleted(const MacroSequence& sequence);
  void playbackProgress(int eventIndex, int eventCount);
  void playbackFinished();
  void stepProgress(int stepIndex, int round, int completedRounds);
  void statusChanged(const QString& status);
  void failed(const QString& reason);

 private slots:
  void handleRecordedEvent(const MacroEvent& event);
  void handleRecordingFailure(const QString& reason);
  void dispatchDueEvents();

 private:
  void setState(MacroControllerState state);
  void abortRecording(const QString& reason, bool reportFailure);
  void finishPlayback(const QString& status, bool completed,
                      const QString& failure = {});
  qint64 playbackElapsedUs() const;
  void scheduleNextPlaybackEvent();

  MacroRecorder* recorder_ = nullptr;
  MacroPlayer* player_ = nullptr;
  AutomationCoordinator* coordinator_ = nullptr;
  MacroControllerState state_ = MacroControllerState::Idle;
  MacroRecordingOptions recordingOptions_;
  MacroSequence sequence_;
  QTimer playbackTimer_;
  QElapsedTimer playbackClock_;
  qsizetype eventIndex_ = 0;
  int completedRounds_ = 0;
  qint64 roundStartUs_ = 0;
  quint64 playbackGeneration_ = 0;
  bool pauseRequested_ = false;
  bool safeBoundary_ = true;
  qint64 pausedAtUs_ = 0;
  qint64 excludedUs_ = 0;
  int lastStep_ = -1;
  qint64 relativeDeadlineUs_ = 0;
};
