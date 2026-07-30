#include <QSignalSpy>
#include <QTest>
#include <QUuid>

#include "core/ClickBackend.h"
#include "core/ClickController.h"
#include "core/ClickTypes.h"
#include "core/AutomationCoordinator.h"
#include "core/SettingsRepository.h"

class FakeClickBackend : public ClickBackend {
 public:
  bool click(const ClickProfile&) override {
    ++clickCount;
    return clickResult;
  }

  bool keyTap(const ClickProfile&) override {
    ++keyTapCount;
    return keyTapResult;
  }

  QPoint currentCursorPosition() const override {
    return QPoint(50, 60);
  }

  bool hasAccessibilityPermission() const override {
    return accessibilityAllowed;
  }

  void requestAccessibilityPermission() override {
    requestedPermission = true;
  }

  bool clickResult = true;
  bool keyTapResult = true;
  bool accessibilityAllowed = true;
  mutable bool requestedPermission = false;
  int clickCount = 0;
  int keyTapCount = 0;
};

class ClickerTests : public QObject {
  Q_OBJECT

 private slots:
  void clickProfileRoundTrip();
  void controllerFiniteRunCompletes();
  void controllerRejectsWithoutPermission();
  void controllerSendsKeyboardTaps();
  void controllerUsesExclusiveAutomationOwnership();
  void settingsRepositoryCrud();
};

void ClickerTests::clickProfileRoundTrip() {
  ClickProfile profile;
  profile.name = "Daily";
  profile.intervalMs = 250;
  profile.button = ClickButton::Right;
  profile.targetMode = TargetMode::FixedPoint;
  profile.fixedPoint = QPoint(640, 480);
  profile.repeatMode = RepeatMode::Finite;
  profile.repeatCount = 8;
  profile.jitterRadius = 7;
  profile.countdownSeconds = 3;
  profile.alwaysOnTop = true;
  profile.hotkeys.startStop = "Ctrl+F6";
  profile.hotkeys.capturePoint = "Ctrl+F7";
  profile.hotkeys.emergencyStop = "Ctrl+F8";

  const QVariantMap data = ClickTypes::toVariantMap(profile);
  const ClickProfile restored = ClickTypes::fromVariantMap(data);

  QCOMPARE(restored.name, profile.name);
  QCOMPARE(restored.intervalMs, profile.intervalMs);
  QCOMPARE(restored.button, profile.button);
  QCOMPARE(restored.targetMode, profile.targetMode);
  QCOMPARE(restored.fixedPoint, profile.fixedPoint);
  QCOMPARE(restored.repeatMode, profile.repeatMode);
  QCOMPARE(restored.repeatCount, profile.repeatCount);
  QCOMPARE(restored.jitterRadius, profile.jitterRadius);
  QCOMPARE(restored.countdownSeconds, profile.countdownSeconds);
  QCOMPARE(restored.alwaysOnTop, profile.alwaysOnTop);
  QCOMPARE(restored.hotkeys.startStop, profile.hotkeys.startStop);
  QCOMPARE(restored.hotkeys.capturePoint, profile.hotkeys.capturePoint);
  QCOMPARE(restored.hotkeys.emergencyStop, profile.hotkeys.emergencyStop);
}

void ClickerTests::controllerFiniteRunCompletes() {
  FakeClickBackend backend;
  ClickController controller(&backend);

  QSignalSpy runningSpy(&controller, &ClickController::runningChanged);
  QSignalSpy remainingSpy(&controller, &ClickController::remainingClicksChanged);

  ClickProfile profile;
  profile.intervalMs = 10;
  profile.repeatMode = RepeatMode::Finite;
  profile.repeatCount = 3;

  controller.start(profile);
  QTRY_VERIFY_WITH_TIMEOUT(!controller.isRunning(), 500);

  QCOMPARE(backend.clickCount, 3);
  QCOMPARE(controller.currentStatus(), QString("已完成"));
  QVERIFY(runningSpy.count() >= 2);
  QVERIFY(!remainingSpy.isEmpty());
}

void ClickerTests::controllerRejectsWithoutPermission() {
  FakeClickBackend backend;
  backend.accessibilityAllowed = false;
  ClickController controller(&backend);

  QSignalSpy rejectedSpy(&controller, &ClickController::startRejected);

  controller.start(ClickProfile{});

  QCOMPARE(backend.requestedPermission, true);
  QCOMPARE(controller.isRunning(), false);
  QCOMPARE(rejectedSpy.count(), 1);
}

void ClickerTests::controllerSendsKeyboardTaps() {
  FakeClickBackend backend;
  ClickController controller(&backend);
  ClickProfile profile;
  profile.inputMode = InputMode::Keyboard;
  profile.keyboardKey = "Space";
  profile.intervalMs = 10;
  profile.repeatMode = RepeatMode::Finite;
  profile.repeatCount = 2;

  controller.start(profile);
  QTRY_VERIFY_WITH_TIMEOUT(!controller.isRunning(), 500);
  QCOMPARE(backend.keyTapCount, 2);
  QCOMPARE(backend.clickCount, 0);
}

void ClickerTests::controllerUsesExclusiveAutomationOwnership() {
  FakeClickBackend backend;
  AutomationCoordinator coordinator;
  ClickController controller(&backend, &coordinator);
  QSignalSpy rejectedSpy(&controller, &ClickController::startRejected);

  QVERIFY(coordinator.tryAcquire(AutomationActivity::Recording));
  controller.start(ClickProfile{});
  QCOMPARE(rejectedSpy.count(), 1);
  QVERIFY(rejectedSpy.first().first().toString().contains(QStringLiteral("录制")));
  QCOMPARE(backend.clickCount, 0);
  coordinator.release(AutomationActivity::Recording);

  ClickProfile profile;
  profile.intervalMs = 10;
  profile.repeatMode = RepeatMode::Finite;
  profile.repeatCount = 1;
  controller.start(profile);
  QTRY_VERIFY_WITH_TIMEOUT(!controller.isRunning(), 500);
  QCOMPARE(coordinator.activity(), AutomationActivity::Idle);
  QCOMPARE(backend.clickCount, 1);
}

void ClickerTests::settingsRepositoryCrud() {
  const QString appName = QString("QtClickerTest-%1").arg(QUuid::createUuid().toString());
  SettingsRepository repository("OpenAI", appName);

  ClickProfile first;
  first.name = "Alpha";
  first.intervalMs = 111;
  repository.saveProfile(first);
  repository.saveLastUsedProfile(first);

  QVERIFY(repository.hasProfile("Alpha"));
  QVERIFY(repository.loadProfile("Alpha").has_value());
  QVERIFY(repository.loadLastUsedProfile().has_value());

  QVERIFY(repository.renameProfile("Alpha", "Beta"));
  QVERIFY(repository.hasProfile("Beta"));
  QVERIFY(!repository.hasProfile("Alpha"));

  const auto loaded = repository.loadProfile("Beta");
  QVERIFY(loaded.has_value());
  QCOMPARE(loaded->intervalMs, 111);

  QVERIFY(repository.deleteProfile("Beta"));
  QVERIFY(!repository.hasProfile("Beta"));
}

QTEST_MAIN(ClickerTests)

#include "ClickerTests.moc"
