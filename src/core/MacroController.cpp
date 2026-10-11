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
  if (isPlaying() && player_)
    player_->cancel();
  if (coordinator_) {
    if (state_ == MacroControllerState::Recording) {
      coordinator_->release(AutomationActivity::Recording);
    } else if (isPlaying()) {
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
    player_->cancel();
    coordinator_->release(AutomationActivity::Playing);
    return false;
  }

  sequence_ = sequence;
  ++playbackGeneration_;
  eventIndex_ = 0;
  completedRounds_ = 0;
  roundStartUs_ = 0;
  excludedUs_ = 0;
  pauseRequested_ = false;
  safeBoundary_ = true;
  lastStep_ = -1;
  playbackClock_.start();
  relativeDeadlineUs_ = scaledOffsetUs(sequence_.events.first().offsetUs, sequence_.playback.speed);
  const auto generation = playbackGeneration_;
  setState(MacroControllerState::Playing);
  if (state_ != MacroControllerState::Playing || generation != playbackGeneration_) return true;
  emit statusChanged("宏回放中");
  if (state_ != MacroControllerState::Playing || generation != playbackGeneration_) return true;
  dispatchDueEvents();
  return true;
}

void MacroController::stop() {
  if (state_ == MacroControllerState::Recording) {
    stopRecording();
  } else if (isPlaying()) {
    finishPlayback("宏回放已停止", false);
  }
}

void MacroController::emergencyStop() {
  if (state_ == MacroControllerState::Recording) {
    abortRecording("宏录制已紧急停止", false);
  } else if (isPlaying()) {
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
  return state_ == MacroControllerState::Playing || isPaused();
}

void MacroController::pause() {
  if (state_ != MacroControllerState::Playing)
    return;
  pauseRequested_ = true;
  if (!safeBoundary_) {
    emit statusChanged("等待当前完整动作结束后暂停；仍可紧急停止");
    return;
  }
  playbackTimer_.stop();
  pausedAtUs_ = playbackElapsedUs();
  player_->releaseAll();
  setState(MacroControllerState::Paused);
  if (isPaused())
    emit statusChanged("流程已暂停");
}

bool MacroController::resume(QString* error) {
  if (!isPaused())
    return false;
  if (!player_->revalidate(error)) {
    finishPlayback("继续失败，流程已停止", false,
                   error ? *error : QString("运行条件已变化，请重新检查目标。"));
    return false;
  }
  excludedUs_ = playbackClock_.nsecsElapsed() / 1000 - pausedAtUs_;
  pauseRequested_ = false;
  const auto generation = playbackGeneration_;
  setState(MacroControllerState::Playing);
  if (state_ == MacroControllerState::Playing && generation == playbackGeneration_)
    scheduleNextPlaybackEvent();
  return true;
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
  // 限制每批补发数量，让停止热键和界面事件有机会得到处理。
  constexpr int maxEventsPerDispatch = 64;
  int dispatched = 0;
  const auto generation = playbackGeneration_;
  qint64 nowUs = playbackElapsedUs();
  while (eventIndex_ < sequence_.events.size() &&
         dispatched < maxEventsPerDispatch) {
    const auto& event = sequence_.events[eventIndex_];
    const qint64 deadlineUs =
        event.relativeDelay
            ? relativeDeadlineUs_
            : roundStartUs_ + scaledOffsetUs(event.offsetUs, sequence_.playback.speed);
    if (deadlineUs > nowUs) break;
    QString error;
    if (!player_->inject(event, &error)) {
      if (event.stepIndex >= 0)
        error = QString("步骤 %1：%2").arg(event.stepIndex + 1).arg(error);
      finishPlayback("宏回放失败", false, error);
      return;
    }
    ++eventIndex_;
    ++dispatched;
    // 手工等待从动作实际完成计时；录制块重新锚定后仍按原时间线回放。
    const qint64 completedAtUs = playbackElapsedUs();
    if (event.timelineAnchor)
      roundStartUs_ = completedAtUs - scaledOffsetUs(event.offsetUs, sequence_.playback.speed);
    if (eventIndex_ < sequence_.events.size() && sequence_.events[eventIndex_].relativeDelay)
      relativeDeadlineUs_ = completedAtUs + scaledOffsetUs(sequence_.events[eventIndex_].delayUs,
                                                           sequence_.playback.speed);
    safeBoundary_ = event.pauseBoundary;
    if (event.stepIndex != lastStep_) {
      lastStep_ = event.stepIndex;
      emit stepProgress(event.stepIndex, completedRounds_ + 1, completedRounds_);
    }
    if (state_ != MacroControllerState::Playing || generation != playbackGeneration_)
      return;
    emit playbackProgress(static_cast<int>(eventIndex_), sequence_.events.size());
    // 同步信号订阅者可能请求停止，不能继续注入或标记完成。
    if (state_ != MacroControllerState::Playing ||
        generation != playbackGeneration_) return;
    if (pauseRequested_ && safeBoundary_) {
      pause();
      return;
    }
    nowUs = playbackElapsedUs();
  }

  if (eventIndex_ >= sequence_.events.size()) {
    ++completedRounds_;
    emit stepProgress(-1, completedRounds_, completedRounds_);
    if (state_ != MacroControllerState::Playing || generation != playbackGeneration_)
      return;
    const bool continuePlaying =
        sequence_.playback.infinite ||
        completedRounds_ < sequence_.playback.repeatCount;
    if (!continuePlaying) {
      finishPlayback("宏回放完成", true);
      return;
    }
    eventIndex_ = 0;
    lastStep_ = -1;
    roundStartUs_ = playbackElapsedUs() +
                    qint64(sequence_.playback.loopDelayMs) * 1000;
    relativeDeadlineUs_ =
        roundStartUs_ + scaledOffsetUs(sequence_.events.first().offsetUs, sequence_.playback.speed);
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
  if (!isPlaying())
    return;
  ++playbackGeneration_;
  playbackTimer_.stop();
  if (player_) player_->cancel();
  if (coordinator_) coordinator_->release(AutomationActivity::Playing);
  setState(MacroControllerState::Idle);
  emit statusChanged(status);
  if (!failure.isEmpty()) emit failed(failure);
  if (completed) emit playbackFinished();
}

qint64 MacroController::playbackElapsedUs() const {
  return playbackClock_.isValid() ? playbackClock_.nsecsElapsed() / 1000 - excludedUs_ : 0;
}

void MacroController::scheduleNextPlaybackEvent() {
  if (state_ != MacroControllerState::Playing || eventIndex_ >= sequence_.events.size()) {
    return;
  }
  const qint64 deadlineUs =
      sequence_.events[eventIndex_].relativeDelay
          ? relativeDeadlineUs_
          : roundStartUs_ +
                scaledOffsetUs(sequence_.events[eventIndex_].offsetUs, sequence_.playback.speed);
  const qint64 remainingUs = std::max<qint64>(0, deadlineUs - playbackElapsedUs());
  const int delayMs = static_cast<int>(std::min<qint64>(
      std::numeric_limits<int>::max(), (remainingUs + 999) / 1000));
  // 积压事件分批补发时至少让出一个计时周期，避免零间隔循环占满事件队列。
  playbackTimer_.start(std::max(1, delayMs));
}
