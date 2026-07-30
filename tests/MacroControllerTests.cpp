#include <QSignalSpy>
#include <QTest>

#include "core/AutomationCoordinator.h"
#include "core/MacroController.h"
#include "core/MacroPlayer.h"
#include "core/MacroRecorder.h"

class FakeMacroRecorder final : public MacroRecorder {
 public:
  using MacroRecorder::MacroRecorder;

  bool start(const MacroRecordingOptions& value, QString* error) override {
    options = value;
    if (!startResult) {
      if (error) *error = "录制器启动失败";
      return false;
    }
    running = true;
    return true;
  }
  void stop() override {
    ++stopCalls;
    running = false;
  }
  bool isRecording() const override { return running; }

  void capture(const MacroEvent& event) { emit eventCaptured(event); }

  MacroRecordingOptions options;
  bool startResult = true;
  bool running = false;
  int stopCalls = 0;
};

class FakeMacroPlayer final : public MacroPlayer {
 public:
  using MacroPlayer::MacroPlayer;

  bool prepare(const MacroSequence& value, QString* error) override {
    preparedSequence = value;
    if (!prepareResult) {
      if (error) *error = "播放器准备失败";
      return false;
    }
    prepared = true;
    return true;
  }
  bool inject(const MacroEvent& event, QString* error) override {
    if (!injectResult) {
      if (error) *error = "注入失败";
      return false;
    }
    injected.append(event);
    return true;
  }
  void releaseAll() override { ++releaseCalls; }
  void cancel() override {
    ++cancelCalls;
    prepared = false;
    releaseAll();
  }

  MacroSequence preparedSequence;
  QVector<MacroEvent> injected;
  bool prepareResult = true;
  bool injectResult = true;
  bool prepared = false;
  int releaseCalls = 0;
  int cancelCalls = 0;
};

namespace {

MacroSequence shortSequence() {
  MacroSequence sequence;
  sequence.id = "00000000-0000-0000-0000-000000000002";
  sequence.name = "短宏";
  sequence.createdAt = QDateTime::currentDateTimeUtc();
  sequence.modifiedAt = sequence.createdAt;
  sequence.durationUs = 2000;
  sequence.playback.speed = 2.0;
  sequence.playback.repeatCount = 2;
  sequence.playback.loopDelayMs = 1;
  MacroEvent down;
  down.type = MacroEventType::KeyDown;
  down.virtualKey = 'A';
  down.scanCode = 0x1e;
  down.offsetUs = 0;
  sequence.events.append(down);
  MacroEvent up = down;
  up.type = MacroEventType::KeyUp;
  up.offsetUs = 2000;
  sequence.events.append(up);
  return sequence;
}

}  // namespace

class MacroControllerTests : public QObject {
  Q_OBJECT

 private slots:
  void recordsCompressesAndReleasesOwnership();
  void playsAtScaledOffsetsAndRepeats();
  void playbackFailureReleasesInputsAndOwnership();
  void rejectsWhenAnotherAutomationActivityOwnsCoordinator();
};

void MacroControllerTests::recordsCompressesAndReleasesOwnership() {
  FakeMacroRecorder recorder;
  FakeMacroPlayer player;
  AutomationCoordinator coordinator;
  MacroController controller(&recorder, &player, &coordinator);
  QSignalSpy completedSpy(&controller, &MacroController::recordingCompleted);

  MacroRecordingOptions options;
  options.targetMode = MacroTargetMode::Global;
  options.reservedHotkeys = {"F9", "F8"};
  QString error;
  QVERIFY2(controller.startRecording(options, &error), qPrintable(error));
  QCOMPARE(coordinator.activity(), AutomationActivity::Recording);

  MacroEvent down;
  down.type = MacroEventType::KeyDown;
  down.offsetUs = 1000;
  down.virtualKey = 'A';
  recorder.capture(down);
  MacroEvent up = down;
  up.type = MacroEventType::KeyUp;
  up.offsetUs = 2000;
  recorder.capture(up);
  controller.stopRecording();

  QCOMPARE(completedSpy.count(), 1);
  const auto sequence =
      qvariant_cast<MacroSequence>(completedSpy.first().first());
  QVERIFY(!sequence.id.isEmpty());
  QCOMPARE(sequence.events.size(), 2);
  QCOMPARE(sequence.durationUs, qint64(2000));
  QCOMPARE(sequence.targetMode, MacroTargetMode::Global);
  QCOMPARE(coordinator.activity(), AutomationActivity::Idle);
  QCOMPARE(controller.state(), MacroControllerState::Idle);
  QCOMPARE(recorder.stopCalls, 1);
}

void MacroControllerTests::playsAtScaledOffsetsAndRepeats() {
  FakeMacroRecorder recorder;
  FakeMacroPlayer player;
  AutomationCoordinator coordinator;
  MacroController controller(&recorder, &player, &coordinator);
  QSignalSpy finishedSpy(&controller, &MacroController::playbackFinished);
  const MacroSequence sequence = shortSequence();
  QString error;

  QCOMPARE(MacroController::scaledOffsetUs(250000, 2.0), qint64(125000));
  QCOMPARE(MacroController::scaledOffsetUs(100000, 0.5), qint64(200000));
  QVERIFY2(controller.startPlayback(sequence, &error), qPrintable(error));
  QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 500);

  QCOMPARE(player.injected.size(), 4);
  QCOMPARE(player.injected[0].type, MacroEventType::KeyDown);
  QCOMPARE(player.injected[1].type, MacroEventType::KeyUp);
  QCOMPARE(player.injected[2].type, MacroEventType::KeyDown);
  QCOMPARE(player.injected[3].type, MacroEventType::KeyUp);
  QCOMPARE(player.cancelCalls, 1);
  QCOMPARE(coordinator.activity(), AutomationActivity::Idle);
  QCOMPARE(controller.state(), MacroControllerState::Idle);
}

void MacroControllerTests::playbackFailureReleasesInputsAndOwnership() {
  FakeMacroRecorder recorder;
  FakeMacroPlayer player;
  player.injectResult = false;
  AutomationCoordinator coordinator;
  MacroController controller(&recorder, &player, &coordinator);
  QSignalSpy failedSpy(&controller, &MacroController::failed);
  QString error;

  QVERIFY(controller.startPlayback(shortSequence(), &error));
  QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 200);
  QCOMPARE(player.cancelCalls, 1);
  QVERIFY(player.releaseCalls >= 1);
  QCOMPARE(coordinator.activity(), AutomationActivity::Idle);
  QCOMPARE(controller.state(), MacroControllerState::Idle);
}

void MacroControllerTests::rejectsWhenAnotherAutomationActivityOwnsCoordinator() {
  FakeMacroRecorder recorder;
  FakeMacroPlayer player;
  AutomationCoordinator coordinator;
  QVERIFY(coordinator.tryAcquire(AutomationActivity::Clicking));
  MacroController controller(&recorder, &player, &coordinator);
  QString error;

  QVERIFY(!controller.startRecording(MacroRecordingOptions{}, &error));
  QVERIFY(error.contains("连点"));
  QVERIFY(!recorder.running);
  QCOMPARE(controller.state(), MacroControllerState::Idle);
}

QTEST_GUILESS_MAIN(MacroControllerTests)

#include "MacroControllerTests.moc"
