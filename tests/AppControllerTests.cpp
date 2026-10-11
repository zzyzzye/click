#include <QCheckBox>
#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QComboBox>
#include <QInputDialog>
#include <QListWidget>
#include <QMainWindow>
#include <QLabel>
#include <QMessageBox>
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
  bool inject(const MacroEvent& event, QString* error) override {
    if (failInjection) {
      if (error) *error = "测试输入失败";
      return false;
    }
    injected.append(event);
    return true;
  }
  void releaseAll() override {}
  void cancel() override {}
  MacroSequence prepared;
  bool failInjection = false;
  QVector<MacroEvent> injected;
};

class AppControllerTests : public QObject {
  Q_OBJECT

 private slots:
  void profileControllerLoadsAndBroadcastsProfile();
  void managingAnotherPresetPreservesActiveProfile();
  void hotkeyControllerOwnsRegistrationAndForwardsIntent();
  void clickControllerCapturesAndStopsCleanly();
  void macroControllerRecordsAndPersistsSequence();
  void workflowUiControllerAppliesMutualExclusion();
};

void AppControllerTests::managingAnotherPresetPreservesActiveProfile() {
  const QString appName = QString("PresetLifecycleTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("ClickFlow", appName);
  auto* storage = repository.get();
  ClickProfile active, other;
  active.name = "正在使用";
  active.intervalMs = 123;
  other.name = "其他预设";
  other.intervalMs = 789;
  QVERIFY(storage->saveProfile(active));
  QVERIFY(storage->saveProfile(other));
  QVERIFY(storage->saveLastUsedProfile(active));
  QMainWindow window;
  CaptionBar caption("测试", &window);
  ControllerFakeWindowStyle style;
  WindowChromeController chrome(&window, &caption, &style);
  ClickSettingsPage clickPage;
  HotkeySettingsPage hotkeyPage;
  PresetsAboutPage presetsPage;
  ActionBar actionBar;
  ProfileController controller(std::move(repository), &clickPage, &hotkeyPage, &presetsPage,
                               &actionBar, &chrome, &window);
  controller.initialize();
  auto* list = presetsPage.findChild<QListWidget*>("presetList");
  list->setCurrentItem(list->findItems(other.name, Qt::MatchExactly).first());
  QTimer::singleShot(0, &window, [] {
    if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
      dialog->setTextValue("已改名");
      dialog->accept();
    }
  });
  emit presetsPage.renameRequested();
  QCOMPARE(controller.currentProfile().name, active.name);
  QCOMPARE(controller.currentProfile().intervalMs, active.intervalMs);
  QVERIFY(storage->hasProfile("已改名"));
  emit presetsPage.saveRequested();
  QCOMPARE(storage->loadProfile("已改名")->intervalMs, other.intervalMs);
  list->setCurrentItem(list->findItems("已改名", Qt::MatchExactly).first());
  QTimer::singleShot(0, &window, [] {
    if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      dialog->button(QMessageBox::Yes)->click();
  });
  emit presetsPage.deleteRequested();
  QVERIFY(!storage->hasProfile("已改名"));
  QCOMPARE(controller.currentProfile().name, active.name);
  QCOMPARE(storage->loadLastUsedProfile()->name, active.name);
  // 修改当前预设名称后，重启恢复的名称也应同步更新。
  list->setCurrentItem(list->findItems(active.name, Qt::MatchExactly).first());
  QTimer::singleShot(0, &window, [] {
    if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
      dialog->setTextValue("当前新名称");
      dialog->accept();
    }
  });
  emit presetsPage.renameRequested();
  QCOMPARE(controller.currentProfile().name, QString("当前新名称"));
  QCOMPARE(storage->loadLastUsedProfile()->name, QString("当前新名称"));
  QVERIFY(storage->deleteProfile("当前新名称"));
}

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
  auto player = std::make_unique<ControllerFakeMacroPlayer>();
  auto* observedPlayer = player.get();
  services.player = std::move(player);
  int nameRequests = 0;

  MacroWorkflowController controller(
      std::move(services), std::make_unique<MacroRepository>(directory.path()),
      &automation, &macroPage, &statusStrip,
      [&profile] { return profile; }, [](QWidget*) { return true; },
      [&nameRequests](QWidget*) { return ++nameRequests == 1 ? QString() : QString("控制器录制"); }, [] { return true; },
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

  QVERIFY(controller.hasDraft());
  QVERIFY(MacroRepository(directory.path()).loadAll().isEmpty());
  auto* save = macroPage.findChild<QPushButton*>("macroSaveDraftButton");
  QVERIFY(save);
  save->click();
  QVERIFY(controller.hasDraft());
  QVERIFY(MacroRepository(directory.path()).loadAll().isEmpty());
  // 用普通文件占用保存目录，验证失败不会丢掉草稿，恢复目录后可重试。
  QVERIFY(QDir().rmdir(directory.path()));
  QFile blocker(directory.path());
  QVERIFY(blocker.open(QIODevice::WriteOnly));
  blocker.close();
  save->click();
  QVERIFY(controller.hasDraft());
  QVERIFY(blocker.remove());
  QVERIFY(QDir().mkpath(directory.path()));
  auto* trial = macroPage.findChild<QPushButton*>("macroTrialButton");
  QVERIFY(trial);
  auto* repeat = macroPage.findChild<QComboBox*>("macroRepeatModeCombo");
  repeat->setCurrentIndex(repeat->findData(true));
  trial->click();
  QTRY_COMPARE(controller.state(), MacroControllerState::Idle);
  QCOMPARE(observedPlayer->injected.size(), 2);
  QVERIFY(!observedPlayer->prepared.playback.infinite);
  QCOMPARE(observedPlayer->prepared.playback.repeatCount, 1);
  QVERIFY(controller.hasDraft());
  QVERIFY(MacroRepository(directory.path()).loadAll().isEmpty());
  // 取消替换草稿时，原录制内容仍可继续保存。
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (prompt) prompt->done(QMessageBox::Cancel);
  });
  controller.toggleRecording();
  QVERIFY(!observedRecorder->running);
  QVERIFY(controller.hasDraft());
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (prompt) prompt->done(QMessageBox::Save);
  });
  QVERIFY(controller.resolveUnsavedDraft());
  QVERIFY(!controller.hasDraft());
  const QVector<MacroSequence> saved =
      MacroRepository(directory.path()).loadAll();
  QCOMPARE(saved.size(), 1);
  QCOMPARE(saved.first().name, QString("控制器录制"));
  const auto originalFile = MacroTypes::toJson(saved.first());
  trial->click();
  QTRY_COMPARE(controller.state(), MacroControllerState::Idle);
  QCOMPARE(MacroTypes::toJson(MacroRepository(directory.path()).loadAll().first()), originalFile);
  observedPlayer->failInjection = true;
  trial->click();
  QTRY_COMPARE(controller.state(), MacroControllerState::Idle);
  QCOMPARE(macroPage.findChild<QLabel*>("macroErrorLabel")->text(), QString("测试输入失败"));
  observedPlayer->failInjection = false;
  controller.toggleRecording();
  observedRecorder->capture(down);
  observedRecorder->capture(up);
  controller.toggleRecording();
  QVERIFY(controller.hasDraft());
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (prompt) prompt->done(QMessageBox::Discard);
  });
  QVERIFY(controller.resolveUnsavedDraft());
  QVERIFY(!controller.hasDraft());
  QCOMPARE(MacroRepository(directory.path()).loadAll().size(), 1);
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
