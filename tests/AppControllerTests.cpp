#include <QCheckBox>
#include <QComboBox>
#include <QMainWindow>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "app/controllers/ClickWorkflowController.h"
#include "app/controllers/HotkeyController.h"
#include "app/controllers/MacroWorkflowController.h"
#include "app/controllers/ProfileController.h"
#include "app/controllers/WindowChromeController.h"
#include "app/controllers/WorkflowUiStateController.h"
#include "app/pages/ClickSettingsPage.h"
#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/MacroRecordingPage.h"
#include "app/pages/PresetsAboutPage.h"
#include "app/widgets/ActionBar.h"
#include "app/widgets/CaptionBar.h"
#include "app/widgets/StatusStrip.h"
#include "core/AutomationCoordinator.h"
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"
#include "core/MacroPlayer.h"
#include "core/MacroRecorder.h"
#include "core/MacroRepository.h"
#include "core/SettingsRepository.h"
#include "core/WindowService.h"
#include "platform/PlatformServices.h"
#include "platform/WindowStyleService.h"

class ControllerFakeClickBackend final : public ClickBackend {
 public:
  bool click(const ClickProfile&) override { return true; }
  bool keyTap(const ClickProfile&) override { return true; }
  QPoint currentCursorPosition() const override { return QPoint(25, 35); }
  bool hasAccessibilityPermission() const override { return true; }
  void requestAccessibilityPermission() override { ++permissionRequests; }
  int permissionRequests = 0;
};

class ControllerFakeHotkeyService final : public HotkeyService {
 public:
  bool registerHotkeys(const ClickProfile& profile) override {
    ++registerCount;
    lastProfile = profile;
    return registrationResult;
  }
  void unregisterAll() override { ++unregisterCount; }
  QString backendName() const override { return "Fake"; }
  void fireStartStop() { emit startStopPressed(); }
  bool registrationResult = true;
  int registerCount = 0;
  int unregisterCount = 0;
  ClickProfile lastProfile;
};

class ControllerFakeWindowStyle final : public WindowStyleService {
 public:
  void prepare(QWidget*) override {}
  void apply(QWidget*) override { ++applyCount; }
  bool usesBackdrop() const override { return false; }
  int applyCount = 0;
};

class ControllerFakeWindowService final : public WindowService {
 public:
  QVector<WindowTarget> availableWindows() const override { return windows; }
  std::optional<WindowTarget> windowAt(const QPoint&) const override {
    return windows.isEmpty() ? std::nullopt
                             : std::optional<WindowTarget>(windows.first());
  }
  std::optional<WindowTarget> resolve(const WindowTarget& target,
                                      QString*) const override { return target; }
  bool isAlive(const WindowTarget&) const override { return true; }
  bool isForeground(const WindowTarget&) const override { return true; }
  bool activate(const WindowTarget&) override { return true; }
  QSize clientSize(const WindowTarget& target) const override {
    return target.clientSize;
  }
  std::optional<QPoint> screenToClient(const WindowTarget&,
                                       const QPoint& point) const override {
    return point;
  }
  std::optional<QPoint> clientToScreen(const WindowTarget&,
                                       const QPoint& point) const override {
    return point;
  }
  QRect virtualDesktopRect() const override { return QRect(0, 0, 1920, 1080); }
  QString displayName(const WindowTarget& target) const override {
    return target.title;
  }
  QVector<WindowTarget> windows;
};

class ControllerFakeMacroRecorder final : public MacroRecorder {
 public:
  bool start(const MacroRecordingOptions&, QString*) override {
    running = true;
    return true;
  }
  void stop() override { running = false; }
  bool isRecording() const override { return running; }
  void capture(const MacroEvent& event) { emit eventCaptured(event); }
  bool running = false;
};

class ControllerFakeMacroPlayer final : public MacroPlayer {
 public:
  bool prepare(const MacroSequence& sequence, QString*) override {
    prepared = sequence;
    return true;
  }
  bool inject(const MacroEvent& event, QString*) override {
    injected.append(event);
    return true;
  }
  void releaseAll() override {}
  void cancel() override {}
  MacroSequence prepared;
  QVector<MacroEvent> injected;
};

class AppControllerTests : public QObject {
  Q_OBJECT

 private slots:
  void profileControllerLoadsAndBroadcastsProfile();
  void hotkeyControllerOwnsRegistrationAndForwardsIntent();
  void clickControllerCapturesAndStopsCleanly();
  void macroControllerRecordsAndPersistsSequence();
  void workflowUiControllerAppliesMutualExclusion();
};

void AppControllerTests::profileControllerLoadsAndBroadcastsProfile() {
  const QString appName =
      QString("ProfileControllerTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("ClickFlow", appName);
  ClickProfile expected;
  expected.name = "测试配置";
  expected.intervalMs = 321;
  expected.alwaysOnTop = true;
  repository->saveLastUsedProfile(expected);

  QMainWindow window;
  CaptionBar caption("测试", &window);
  ControllerFakeWindowStyle style;
  WindowChromeController chrome(&window, &caption, &style);
  ClickSettingsPage clickPage;
  HotkeySettingsPage hotkeyPage;
  PresetsAboutPage presetsPage;
  ActionBar actionBar;
  ProfileController controller(std::move(repository), &clickPage, &hotkeyPage,
                               &presetsPage, &actionBar, &chrome, &window);
  QSignalSpy applied(&controller, &ProfileController::profileApplied);

  controller.initialize();

  QCOMPARE(applied.count(), 1);
  const ClickProfile actual = controller.currentProfile();
  QCOMPARE(actual.name, expected.name);
  QCOMPARE(actual.intervalMs, expected.intervalMs);
  QVERIFY(actual.alwaysOnTop);
  QVERIFY(!controller.macroSafetyAcknowledged());
  controller.acknowledgeMacroSafety();
  QVERIFY(controller.macroSafetyAcknowledged());
}

void AppControllerTests::hotkeyControllerOwnsRegistrationAndForwardsIntent() {
  ClickProfile profile;
  HotkeySettingsPage hotkeyPage;
  MacroRecordingPage macroPage;
  ActionBar actionBar;
  auto service = std::make_unique<ControllerFakeHotkeyService>();
  auto* observed = service.get();
  HotkeyController controller(std::move(service), &hotkeyPage, &macroPage,
                              &actionBar, [&profile] { return profile; });
  QSignalSpy toggleSpy(&controller, &HotkeyController::clickToggleRequested);

  hotkeyPage.setProfile(profile);
  controller.applyProfile(profile);
  auto* activation =
      hotkeyPage.findChild<QCheckBox*>("globalHotkeysEnabledCheck");
  QVERIFY(activation);
  QVERIFY(QMetaObject::invokeMethod(&hotkeyPage, "activationRequested",
                                    Q_ARG(bool, true)));
  QCOMPARE(observed->registerCount, 1);
  QVERIFY(controller.isEnabled());

  observed->fireStartStop();
  QCOMPARE(toggleSpy.count(), 1);

  QVERIFY(QMetaObject::invokeMethod(&hotkeyPage, "activationRequested",
                                    Q_ARG(bool, false)));
  QVERIFY(!controller.isEnabled());
  QVERIFY(observed->unregisterCount >= 1);
}

void AppControllerTests::clickControllerCapturesAndStopsCleanly() {
  AutomationCoordinator automation;
  ClickProfile profile;
  profile.intervalMs = 1000;
  profile.countdownSeconds = 0;
  ClickSettingsPage clickPage;
  ActionBar actionBar;
  StatusStrip statusStrip;
  auto backend = std::make_unique<ControllerFakeClickBackend>();
  ClickWorkflowController controller(
      std::move(backend), &automation, &clickPage, &actionBar, &statusStrip,
      [&profile] { return profile; }, &clickPage);
  QSignalSpy runningSpy(&controller,
                        &ClickWorkflowController::runningChanged);

  controller.capturePoint();
  QCOMPARE(clickPage.findChild<QSpinBox*>("fixedXSpin")->value(), 25);
  QCOMPARE(clickPage.findChild<QSpinBox*>("fixedYSpin")->value(), 35);

  controller.toggle();
  QVERIFY(controller.isRunning());
  controller.emergencyStop();
  QVERIFY(!controller.isRunning());
  QVERIFY(runningSpy.count() >= 2);
}

void AppControllerTests::macroControllerRecordsAndPersistsSequence() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  AutomationCoordinator automation;
  MacroRecordingPage macroPage;
  StatusStrip statusStrip;
  ClickProfile profile;

  MacroPlatformServices services;
  services.windowService = std::make_unique<ControllerFakeWindowService>();
  auto recorder = std::make_unique<ControllerFakeMacroRecorder>();
  auto* observedRecorder = recorder.get();
  services.recorder = std::move(recorder);
  services.player = std::make_unique<ControllerFakeMacroPlayer>();

  MacroWorkflowController controller(
      std::move(services), std::make_unique<MacroRepository>(directory.path()),
      &automation, &macroPage, &statusStrip,
      [&profile] { return profile; }, [](QWidget*) { return true; },
      [](QWidget*) { return QString("控制器录制"); }, [] { return true; },
      [] {}, [] { return quintptr{999}; }, &macroPage);
  controller.initialize();
  controller.toggleRecording();
  QVERIFY(observedRecorder->running);

  MacroEvent down;
  down.type = MacroEventType::KeyDown;
  down.offsetUs = 1000;
  down.virtualKey = 'A';
  observedRecorder->capture(down);
  MacroEvent up = down;
  up.type = MacroEventType::KeyUp;
  up.offsetUs = 2000;
  observedRecorder->capture(up);
  controller.toggleRecording();

  const QVector<MacroSequence> saved =
      MacroRepository(directory.path()).loadAll();
  QCOMPARE(saved.size(), 1);
  QCOMPARE(saved.first().name, QString("控制器录制"));
}

void AppControllerTests::workflowUiControllerAppliesMutualExclusion() {
  ClickSettingsPage clickPage;
  HotkeySettingsPage hotkeyPage;
  MacroRecordingPage macroPage;
  PresetsAboutPage presetsPage;
  ActionBar actionBar;
  WorkflowUiStateController controller(&clickPage, &hotkeyPage, &macroPage,
                                       &presetsPage, &actionBar, true);
  auto* targetMode =
      clickPage.findChild<QComboBox*>("targetModeCombo");
  QVERIFY(targetMode);
  QVERIFY(targetMode->isEnabled());

  controller.setClickRunning(true);
  QVERIFY(!targetMode->isEnabled());
  QVERIFY(actionBar.isEnabled());

  controller.setClickRunning(false);
  controller.setMacroState(MacroControllerState::Recording);
  QVERIFY(!targetMode->isEnabled());
  QVERIFY(!actionBar.isEnabled());

  controller.setMacroState(MacroControllerState::Idle);
  QVERIFY(targetMode->isEnabled());
  QVERIFY(actionBar.isEnabled());
}

QTEST_MAIN(AppControllerTests)
#include "AppControllerTests.moc"
