#include "core/MacroController.h"

#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/MacroCompressor.h"

MacroController::MacroController(MacroRecorder* recorder, MacroPlayer* player,
                                 AutomationCoordinator* coordinator,
                                 QObject* parent)
    : QObject(parent),
      recorder_(recorder),
      player_(player),
      coordinator_(coordinator) {
  playbackTimer_.setSingleShot(true);
  playbackTimer_.setTimerType(Qt::PreciseTimer);
  connect(&playbackTimer_, &QTimer::timeout, this,
          &MacroController::dispatchDueEvents);
  if (recorder_) {
    connect(recorder_, &MacroRecorder::eventCaptured, this,
            &MacroController::handleRecordedEvent);
    connect(recorder_, &MacroRecorder::targetLost, this,
            &MacroController::handleRecordingFailure);
    connect(recorder_, &MacroRecorder::recordingFailed, this,
            &MacroController::handleRecordingFailure);
  }
}

MacroController::~MacroController() {
  if (state_ == MacroControllerState::Recording && recorder_) recorder_->stop();
  if (state_ == MacroControllerState::Playing && player_) player_->cancel();
  if (coordinator_) {
    if (state_ == MacroControllerState::Recording) {
      coordinator_->release(AutomationActivity::Recording);
    } else if (state_ == MacroControllerState::Playing) {
      coordinator_->release(AutomationActivity::Playing);
    }
  }
}

bool MacroController::startRecording(const MacroRecordingOptions& options,
                                     QString* error) {
  if (state_ != MacroControllerState::Idle || !recorder_ || !coordinator_) {
    if (error) *error = "宏录制当前不可用。";
    return false;
  }
  QString ownershipError;
  if (!coordinator_->tryAcquire(AutomationActivity::Recording, &ownershipError)) {
    if (error) *error = ownershipError;
    return false;
  }

  recordingOptions_ = options;
  sequence_ = {};
  sequence_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  sequence_.name = "未命名宏";
  sequence_.createdAt = QDateTime::currentDateTimeUtc();
  sequence_.modifiedAt = sequence_.createdAt;
  sequence_.targetMode = options.targetMode;
  sequence_.target = options.target;
  if (!recorder_->start(options, error)) {
    coordinator_->release(AutomationActivity::Recording);
    return false;
  }
  setState(MacroControllerState::Recording);
  emit statusChanged("宏录制中");
  return true;
}

void MacroController::stopRecording() {
  if (state_ != MacroControllerState::Recording) return;
  if (recorder_) recorder_->stop();
  sequence_.events = MacroCompressor::removeReservedTail(
      sequence_.events, recordingOptions_.reservedHotkeys);
  sequence_.events = MacroCompressor::compress(sequence_.events);
  if (sequence_.events.isEmpty()) {
    abortRecording("没有录制到可保存的键鼠操作。", true);
    return;
  }
  sequence_.durationUs = sequence_.events.last().offsetUs;
  sequence_.modifiedAt = QDateTime::currentDateTimeUtc();
  coordinator_->release(AutomationActivity::Recording);
  setState(MacroControllerState::Idle);
  emit statusChanged("宏录制完成");
  emit recordingCompleted(sequence_);
}

bool MacroController::startPlayback(const MacroSequence& sequence,
                                    QString* error) {
  if (state_ != MacroControllerState::Idle || !player_ || !coordinator_) {
    if (error) *error = "宏回放当前不可用。";
    return false;
  }
  QString validationError;
  if (!MacroTypes::validate(sequence, &validationError)) {
    if (error) *error = validationError;
    return false;
  }
  QString ownershipError;
  if (!coordinator_->tryAcquire(AutomationActivity::Playing, &ownershipError)) {
    if (error) *error = ownershipError;
    return false;
  }
  if (!player_->prepare(sequence, error)) {
    coordinator_->release(AutomationActivity::Playing);
    return false;
  }

  sequence_ = sequence;
  eventIndex_ = 0;
  completedRounds_ = 0;
  roundStartUs_ = 0;
  playbackClock_.start();
  setState(MacroControllerState::Playing);
  emit statusChanged("宏回放中");
  dispatchDueEvents();
  return true;
}

void MacroController::stop() {
  if (state_ == MacroControllerState::Recording) {
    stopRecording();
  } else if (state_ == MacroControllerState::Playing) {
    finishPlayback("宏回放已停止", false);
  }
}

void MacroController::emergencyStop() {
  if (state_ == MacroControllerState::Recording) {
    abortRecording("宏录制已紧急停止", false);
  } else if (state_ == MacroControllerState::Playing) {
    finishPlayback("宏回放已紧急停止", false);
  }
}

MacroControllerState MacroController::state() const {
  return state_;
}

bool MacroController::isRecording() const {
  return state_ == MacroControllerState::Recording;
}

bool MacroController::isPlaying() const {
  return state_ == MacroControllerState::Playing;
}

const MacroSequence& MacroController::currentSequence() const {
  return sequence_;
}

qint64 MacroController::scaledOffsetUs(qint64 recordedOffsetUs, double speed) {
  if (recordedOffsetUs <= 0 || speed <= 0.0) return 0;
  return static_cast<qint64>(std::llround(recordedOffsetUs / speed));
}

void MacroController::handleRecordedEvent(const MacroEvent& event) {
  if (state_ != MacroControllerState::Recording) return;
  if (!sequence_.events.isEmpty() &&
      event.offsetUs < sequence_.events.last().offsetUs) {
    handleRecordingFailure("录制事件时间顺序异常，录制已停止。");
    return;
  }
  sequence_.events.append(event);
  emit recordingProgress(event.offsetUs, sequence_.events.size());
}

void MacroController::handleRecordingFailure(const QString& reason) {
  if (state_ != MacroControllerState::Recording) return;
  abortRecording(reason, true);
}

void MacroController::dispatchDueEvents() {
  if (state_ != MacroControllerState::Playing) return;
  qint64 nowUs = playbackElapsedUs();
  while (eventIndex_ < sequence_.events.size()) {
    const auto& event = sequence_.events[eventIndex_];
    const qint64 deadlineUs =
        roundStartUs_ + scaledOffsetUs(event.offsetUs, sequence_.playback.speed);
    if (deadlineUs > nowUs) break;
    QString error;
    if (!player_->inject(event, &error)) {
      finishPlayback("宏回放失败", false, error);
      return;
    }
    ++eventIndex_;
    emit playbackProgress(static_cast<int>(eventIndex_), sequence_.events.size());
    nowUs = playbackElapsedUs();
  }

  if (eventIndex_ >= sequence_.events.size()) {
    ++completedRounds_;
    const bool continuePlaying =
        sequence_.playback.infinite ||
        completedRounds_ < sequence_.playback.repeatCount;
    if (!continuePlaying) {
      finishPlayback("宏回放完成", true);
      return;
    }
    eventIndex_ = 0;
    roundStartUs_ = playbackElapsedUs() +
                    qint64(sequence_.playback.loopDelayMs) * 1000;
  }
  scheduleNextPlaybackEvent();
}

void MacroController::setState(MacroControllerState state) {
  if (state_ == state) return;
  state_ = state;
  emit stateChanged(state_);
}

void MacroController::abortRecording(const QString& reason, bool reportFailure) {
  if (state_ != MacroControllerState::Recording) return;
  if (recorder_) recorder_->stop();
  if (coordinator_) coordinator_->release(AutomationActivity::Recording);
  sequence_ = {};
  setState(MacroControllerState::Idle);
  emit statusChanged(reason);
  if (reportFailure) emit failed(reason);
}

void MacroController::finishPlayback(const QString& status, bool completed,
                                     const QString& failure) {
  if (state_ != MacroControllerState::Playing) return;
  playbackTimer_.stop();
  if (player_) player_->cancel();
  if (coordinator_) coordinator_->release(AutomationActivity::Playing);
  setState(MacroControllerState::Idle);
  emit statusChanged(status);
  if (!failure.isEmpty()) emit failed(failure);
  if (completed) emit playbackFinished();
}

qint64 MacroController::playbackElapsedUs() const {
  return playbackClock_.isValid() ? playbackClock_.nsecsElapsed() / 1000 : 0;
}

void MacroController::scheduleNextPlaybackEvent() {
  if (state_ != MacroControllerState::Playing || eventIndex_ >= sequence_.events.size()) {
    return;
  }
  const qint64 deadlineUs =
      roundStartUs_ +
      scaledOffsetUs(sequence_.events[eventIndex_].offsetUs,
                     sequence_.playback.speed);
  const qint64 remainingUs = std::max<qint64>(0, deadlineUs - playbackElapsedUs());
  const int delayMs = static_cast<int>(std::min<qint64>(
      std::numeric_limits<int>::max(), (remainingUs + 999) / 1000));
  playbackTimer_.start(delayMs);
}
