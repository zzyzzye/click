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
  QVector<QRect> screenRects() const override {
    return screens;
  }
  bool supportsKey(const QString&) const override {
    return keySupported;
  }

  QVector<QRect> screens{QRect(-200, 0, 200, 200), QRect(100, 0, 200, 200)};
  bool keySupported = true;
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
  void stopFromProgressRemainsStopped();
  void stopDuringStartDoesNotStartCountdown();
  void invalidTargetNeverStartsInput();
  void changedConditionsStopBeforeNextInput();
  void countdownRevalidatesBeforeFirstInput();
  void stopAtCountdownTickCannotOverwriteStatus();
  void settingsRepositoryCrud();
  void invalidPresetNamesDoNotCreateNestedGroups();
  void settingsRepositoryPersistsMacroSafetyAcknowledgement();
};

void ClickerTests::stopAtCountdownTickCannotOverwriteStatus() {
  FakeClickBackend backend;
  ClickController controller(&backend);
  ClickProfile profile;
  profile.countdownSeconds = 3;
  controller.start(profile);
  connect(&controller, &ClickController::countdownChanged, &controller, [&](int remaining) {
    if (remaining == 2)
      controller.stop();
  });
  QVERIFY(QMetaObject::invokeMethod(&controller, "handleCountdownTick", Qt::DirectConnection));
  QVERIFY(!controller.isRunning());
  QCOMPARE(controller.currentStatus(), QString("已停止"));
  QCOMPARE(backend.clickCount, 0);
}

void ClickerTests::invalidPresetNamesDoNotCreateNestedGroups() {
  const QString appName = QString("PresetNamesTest-%1").arg(QUuid::createUuid().toString());
  SettingsRepository repository("ClickFlow", appName);
  for (const QString& name : {QString(""), QString(" "), QString("a/b"), QString("a\\b")}) {
    ClickProfile profile;
    profile.name = name;
    QVERIFY(!repository.saveProfile(profile));
    QVERIFY(!repository.hasProfile(name));
  }
  QVERIFY(repository.profileNames().isEmpty());
  ClickProfile profile;
  profile.name = "正常名称";
  QVERIFY(repository.saveProfile(profile));
  QVERIFY(!repository.renameProfile(profile.name, "无效/名称"));
  QVERIFY(repository.hasProfile(profile.name));
  QVERIFY(repository.deleteProfile(profile.name));
}

void ClickerTests::invalidTargetNeverStartsInput() {
  FakeClickBackend backend;
  AutomationCoordinator coordinator;
  ClickController controller(&backend, &coordinator);
  QSignalSpy rejected(&controller, &ClickController::startRejected);
  ClickProfile profile;
  profile.targetMode = TargetMode::FixedPoint;
  profile.fixedPoint = {50, 50}; // 两块屏幕之间的空隙。
  controller.start(profile);
  QCOMPARE(rejected.count(), 1);
  QCOMPARE(backend.clickCount, 0);
  QCOMPARE(coordinator.activity(), AutomationActivity::Idle);
  profile.inputMode = InputMode::Keyboard;
  backend.keySupported = false;
  controller.start(profile);
  QCOMPARE(rejected.count(), 2);
  QCOMPARE(backend.keyTapCount, 0);
  profile.inputMode = InputMode::Mouse;
  profile.fixedPoint = {-100, 50};
  profile.repeatMode = RepeatMode::Finite;
  profile.repeatCount = 1;
  controller.start(profile);
  QCOMPARE(backend.clickCount, 1);
  QCOMPARE(controller.currentStatus(), QString("已完成"));
}

void ClickerTests::changedConditionsStopBeforeNextInput() {
  for (bool permissionLoss : {true, false}) {
    FakeClickBackend backend;
    AutomationCoordinator coordinator;
    ClickController controller(&backend, &coordinator);
    ClickProfile profile;
    profile.intervalMs = 300;
    profile.targetMode = TargetMode::FixedPoint;
    profile.fixedPoint = {-100, 50};
    controller.start(profile);
    QCOMPARE(backend.clickCount, 1);
    if (permissionLoss)
      backend.accessibilityAllowed = false;
    else
      backend.screens = {QRect(-200, 0, 400, 200)};
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isRunning(), 1000);
    QCOMPARE(backend.clickCount, 1);
    QCOMPARE(coordinator.activity(), AutomationActivity::Idle);
    QVERIFY(controller.currentStatus().contains(permissionLoss ? "权限" : "布局"));
  }
}

void ClickerTests::countdownRevalidatesBeforeFirstInput() {
  FakeClickBackend backend;
  ClickController controller(&backend);
  ClickProfile profile;
  profile.countdownSeconds = 1;
  controller.start(profile);
  backend.accessibilityAllowed = false;
  QVERIFY(QMetaObject::invokeMethod(&controller, "handleCountdownTick", Qt::DirectConnection));
  QVERIFY(!controller.isRunning());
  QCOMPARE(backend.clickCount, 0);
  QVERIFY(controller.currentStatus().contains("权限"));
}

void ClickerTests::stopFromProgressRemainsStopped() {
  FakeClickBackend backend;
  AutomationCoordinator coordinator;
  ClickController controller(&backend, &coordinator);
  ClickProfile profile;
  profile.repeatMode = RepeatMode::Finite;
  profile.repeatCount = 1;
  connect(&controller, &ClickController::clicksExecutedChanged, &controller,
          [&controller](int count) { if (count > 0) controller.stop(); });
  controller.start(profile);
  QVERIFY(!controller.isRunning());
  QCOMPARE(controller.currentStatus(), QString("已停止"));
  QCOMPARE(coordinator.activity(), AutomationActivity::Idle);
  QTest::qWait(10);
  QCOMPARE(backend.clickCount, 1);
}

void ClickerTests::stopDuringStartDoesNotStartCountdown() {
  FakeClickBackend backend;
  ClickController controller(&backend);
  ClickProfile profile;
  profile.countdownSeconds = 5;
  connect(&controller, &ClickController::runningChanged, &controller,
          [&controller](bool running) { if (running) controller.emergencyStop(); });
  QSignalSpy countdown(&controller, &ClickController::countdownChanged);
  controller.start(profile);
  QVERIFY(!controller.isRunning());
  QCOMPARE(controller.currentStatus(), QString("已紧急停止"));
  QCOMPARE(backend.clickCount, 0);
  for (const auto& notification : countdown) QCOMPARE(notification.first().toInt(), 0);
}

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
  profile.hotkeys.macroRecord = "Ctrl+F9";
  profile.hotkeys.macroPlayback = "Ctrl+F10";

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
  QCOMPARE(restored.hotkeys.macroRecord, profile.hotkeys.macroRecord);
  QCOMPARE(restored.hotkeys.macroPlayback, profile.hotkeys.macroPlayback);
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
  const QString appName = QString("ClickFlowTest-%1").arg(QUuid::createUuid().toString());
  SettingsRepository repository("ClickFlow", appName);

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

void ClickerTests::settingsRepositoryPersistsMacroSafetyAcknowledgement() {
  const QString appName =
      QString("ClickFlowSafetyTest-%1").arg(QUuid::createUuid().toString());
  SettingsRepository repository("ClickFlow", appName);

  QVERIFY(!repository.macroSafetyAcknowledged());
  repository.setMacroSafetyAcknowledged(true);
  QVERIFY(repository.macroSafetyAcknowledged());
}

QTEST_MAIN(ClickerTests)

#include "ClickerTests.moc"
