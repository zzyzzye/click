#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTest>
#include <QTemporaryDir>
#include <QUuid>
#include <QWheelEvent>

#include <memory>

#include "app/MainWindow.h"
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"
#include "core/MacroPlayer.h"
#include "core/MacroRecorder.h"
#include "core/MacroRepository.h"
#include "core/WindowService.h"
#include "platform/PlatformServices.h"
#if defined(Q_OS_WIN)
#include "platform/windows/WindowsClickBackend.h"
#include "platform/windows/WindowsHotkeyService.h"
#endif
#include "app/widgets/ActionBar.h"
#include "app/widgets/NavigationSidebar.h"
#include "app/widgets/SmoothScrollArea.h"
#include "app/widgets/StatusStrip.h"

class MainWindowFakeClickBackend final : public ClickBackend {
 public:
  bool click(const ClickProfile& profile) override {
    lastProfile = profile;
    ++clickCount;
    return true;
  }

  bool keyTap(const ClickProfile&) override { return true; }

  QPoint currentCursorPosition() const override {
    return QPoint(25, 35);
  }

  bool hasAccessibilityPermission() const override {
    return true;
  }

  void requestAccessibilityPermission() override {}

  ClickProfile lastProfile;
  int clickCount = 0;
};

class MainWindowFakeHotkeyService final : public HotkeyService {
 public:
  using HotkeyService::HotkeyService;

  bool registerHotkeys(const ClickProfile& profile) override {
    ++registerCount;
    lastRegisteredProfile = profile;
    return registrationResult;
  }

  void unregisterAll() override { ++unregisterCount; }

  QString backendName() const override {
    return "Fake";
  }

  bool registrationResult = true;
  int registerCount = 0;
  int unregisterCount = 0;
  ClickProfile lastRegisteredProfile;
};

class MainWindowFakeWindowService final : public WindowService {
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
                                       const QPoint& point) const override { return point; }
  std::optional<QPoint> clientToScreen(const WindowTarget&,
                                       const QPoint& point) const override { return point; }
  QRect virtualDesktopRect() const override { return QRect(0, 0, 1920, 1080); }
  QString displayName(const WindowTarget& target) const override { return target.title; }

  QVector<WindowTarget> windows;
};

class MainWindowFakeMacroRecorder final : public MacroRecorder {
 public:
  using MacroRecorder::MacroRecorder;
  bool start(const MacroRecordingOptions&, QString*) override {
    running = true;
    return true;
  }
  void stop() override { running = false; }
  bool isRecording() const override { return running; }
  void capture(const MacroEvent& event) { emit eventCaptured(event); }
  bool running = false;
};

class MainWindowFakeMacroPlayer final : public MacroPlayer {
 public:
  using MacroPlayer::MacroPlayer;
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

class MainWindowTests : public QObject {
  Q_OBJECT

 private slots:
#if defined(Q_OS_WIN)
  void windowsFactoriesCreateNativeServices();
#endif
  void availableInputHidesPermissionRequest();
  void loadedProfileRoundTripsThroughStart();
  void modeControlsFollowProfileChoices();
  void captureHotkeyUsesCurrentCursor();
  void usesPersistentClickFlowShell();
  void controlChevronResourcesAreAvailable();
  void usesClickFlowControlChrome();
  void smoothScrollUsesContinuousWheelTarget();
  void smoothScrollClampsAtBoundaries();
  void wheelOverComboScrollsTheSettingsPage();
  void macroServicesRecordPersistAndReplay();
  void startsWithGlobalHotkeysDisabled();
  void globalHotkeysRequireManualActivation();
  void registrationFailureReturnsActivationToOff();
};

#if defined(Q_OS_WIN)
void MainWindowTests::windowsFactoriesCreateNativeServices() {
  auto backend = createClickBackend();
  auto hotkeys = createHotkeyService();

  QVERIFY(dynamic_cast<WindowsClickBackend*>(backend.get()));
  QVERIFY(dynamic_cast<WindowsHotkeyService*>(hotkeys.get()));
  auto macros = createMacroPlatformServices();
  QVERIFY(macros.windowService);
  QVERIFY(macros.recorder);
  QVERIFY(macros.player);
}
#endif

void MainWindowTests::availableInputHidesPermissionRequest() {
  const QString appName =
      QString("QtClickerMainWindowTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);

  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository));

  auto* label = window.findChild<QLabel*>("permissionLabel");
  auto* button = window.findChild<QPushButton*>("permissionButton");

  QVERIFY(label);
  QVERIFY(button);
  QCOMPARE(label->text(), QString("输入控制权限：可用"));
  QVERIFY(button->isHidden());
}

void MainWindowTests::loadedProfileRoundTripsThroughStart() {
  const QString appName =
      QString("QtClickerMainWindowTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);

  ClickProfile expected;
  expected.name = "Windows profile";
  expected.intervalMs = 600000;
  expected.button = ClickButton::Right;
  expected.targetMode = TargetMode::FixedPoint;
  expected.fixedPoint = QPoint(640, 480);
  expected.repeatMode = RepeatMode::Finite;
  expected.repeatCount = 8;
  expected.jitterRadius = 7;
  expected.countdownSeconds = 0;
  expected.alwaysOnTop = true;
  expected.hotkeys.startStop = "Ctrl+F6";
  expected.hotkeys.capturePoint = "Ctrl+F7";
  expected.hotkeys.emergencyStop = "Ctrl+F8";
  repository->saveLastUsedProfile(expected);

  auto backend = std::make_unique<MainWindowFakeClickBackend>();
  auto* observed = backend.get();
  MainWindow window(std::move(backend),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository));

  auto* startButton = window.findChild<QPushButton*>("startStopButton");
  QVERIFY(startButton);
  startButton->click();

  QCOMPARE(observed->clickCount, 1);
  const ClickProfile& actual = observed->lastProfile;
  QCOMPARE(actual.name, expected.name);
  QCOMPARE(actual.intervalMs, expected.intervalMs);
  QCOMPARE(actual.button, expected.button);
  QCOMPARE(actual.targetMode, expected.targetMode);
  QCOMPARE(actual.fixedPoint, expected.fixedPoint);
  QCOMPARE(actual.repeatMode, expected.repeatMode);
  QCOMPARE(actual.repeatCount, expected.repeatCount);
  QCOMPARE(actual.jitterRadius, expected.jitterRadius);
  QCOMPARE(actual.alwaysOnTop, expected.alwaysOnTop);
  QCOMPARE(actual.hotkeys.startStop, expected.hotkeys.startStop);
  QCOMPARE(actual.hotkeys.capturePoint, expected.hotkeys.capturePoint);
  QCOMPARE(actual.hotkeys.emergencyStop, expected.hotkeys.emergencyStop);
}

void MainWindowTests::modeControlsFollowProfileChoices() {
  const QString appName =
      QString("QtClickerMainWindowTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository));

  auto* targetMode = window.findChild<QComboBox*>("targetModeCombo");
  auto* fixedX = window.findChild<QSpinBox*>("fixedXSpin");
  auto* repeatMode = window.findChild<QComboBox*>("repeatModeCombo");
  auto* repeatCount = window.findChild<QSpinBox*>("repeatCountSpin");
  QVERIFY(targetMode);
  QVERIFY(fixedX);
  QVERIFY(repeatMode);
  QVERIFY(repeatCount);

  targetMode->setCurrentIndex(
      targetMode->findData(static_cast<int>(TargetMode::FixedPoint)));
  QVERIFY(fixedX->isEnabled());
  targetMode->setCurrentIndex(
      targetMode->findData(static_cast<int>(TargetMode::FollowCursor)));
  QVERIFY(!fixedX->isEnabled());

  repeatMode->setCurrentIndex(
      repeatMode->findData(static_cast<int>(RepeatMode::Finite)));
  QVERIFY(repeatCount->isEnabled());
  repeatMode->setCurrentIndex(
      repeatMode->findData(static_cast<int>(RepeatMode::Infinite)));
  QVERIFY(!repeatCount->isEnabled());
}

void MainWindowTests::captureHotkeyUsesCurrentCursor() {
  const QString appName =
      QString("QtClickerMainWindowTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  auto hotkeys = std::make_unique<MainWindowFakeHotkeyService>();
  auto* hotkeySignals = hotkeys.get();
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::move(hotkeys), std::move(repository));

  hotkeySignals->capturePointPressed();

  auto* targetMode = window.findChild<QComboBox*>("targetModeCombo");
  auto* fixedX = window.findChild<QSpinBox*>("fixedXSpin");
  auto* fixedY = window.findChild<QSpinBox*>("fixedYSpin");
  QVERIFY(targetMode);
  QVERIFY(fixedX);
  QVERIFY(fixedY);
  QCOMPARE(targetMode->currentData().toInt(),
           static_cast<int>(TargetMode::FixedPoint));
  QCOMPARE(fixedX->value(), 25);
  QCOMPARE(fixedY->value(), 35);
}

void MainWindowTests::usesPersistentClickFlowShell() {
  const QString appName =
      QString("QtClickerMainWindowTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository));

  QCOMPARE(window.windowTitle(), QString("ClickFlow"));
  QCOMPARE(window.minimumSize(), QSize(820, 560));
  auto* sidebar = window.findChild<NavigationSidebar*>();
  auto* status = window.findChild<StatusStrip*>();
  auto* actions = window.findChild<ActionBar*>();
  auto* pages = window.findChild<QStackedWidget*>("contentPages");
  QVERIFY(sidebar);
  QVERIFY(status);
  QVERIFY(actions);
  QVERIFY(pages);
  QCOMPARE(sidebar->pageCount(), 4);
  QCOMPARE(pages->count(), 4);
  for (int index = 0; index < pages->count(); ++index) {
    auto* scroll = dynamic_cast<SmoothScrollArea*>(pages->widget(index));
    QVERIFY(scroll);
    QVERIFY(scroll->widgetResizable());
    QCOMPARE(scroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
  }
}

void MainWindowTests::controlChevronResourcesAreAvailable() {
  QVERIFY(QFile::exists(":/clickflow/icons/chevron-down.svg"));
  QVERIFY(QFile::exists(":/clickflow/icons/chevron-up.svg"));
}

void MainWindowTests::usesClickFlowControlChrome() {
  const QString appName =
      QString("QtClickerMainWindowTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository));

  const QString style = window.styleSheet();
  const QString compactStyle = style.simplified();
  QVERIFY(style.contains("QComboBox::down-arrow"));
  QVERIFY(style.contains("QSpinBox::up-button"));
  QVERIFY(style.contains("QSpinBox::down-button"));
  QVERIFY(style.contains(":/clickflow/icons/chevron-down.svg"));
  QVERIFY(style.contains(":/clickflow/icons/chevron-up.svg"));
  QVERIFY(compactStyle.contains(
      "#sidebarNavigation { background: transparent; border: none;"));
}

void MainWindowTests::smoothScrollUsesContinuousWheelTarget() {
  SmoothScrollArea scroll;
  auto* content = new QWidget;
  content->setFixedSize(200, 1000);
  scroll.setWidget(content);
  scroll.resize(200, 200);
  scroll.show();
  QCoreApplication::processEvents();

  auto* bar = scroll.verticalScrollBar();
  QVERIFY(bar->maximum() > 100);

  QWheelEvent first(QPointF(40, 40), QPointF(40, 40), QPoint(), QPoint(0, -120),
                    Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QVERIFY(QCoreApplication::sendEvent(scroll.viewport(), &first));
  QVERIFY(first.isAccepted());
  QTRY_COMPARE_WITH_TIMEOUT(bar->value(), 37, 500);

  QWheelEvent second(QPointF(40, 40), QPointF(40, 40), QPoint(0, -19), QPoint(),
                     Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QVERIFY(QCoreApplication::sendEvent(scroll.viewport(), &second));
  QVERIFY(second.isAccepted());
  QTRY_COMPARE_WITH_TIMEOUT(bar->value(), 56, 500);
}

void MainWindowTests::smoothScrollClampsAtBoundaries() {
  SmoothScrollArea scroll;
  auto* content = new QWidget;
  content->setFixedSize(200, 1000);
  scroll.setWidget(content);
  scroll.resize(200, 200);
  scroll.show();
  QCoreApplication::processEvents();

  auto* bar = scroll.verticalScrollBar();
  QVERIFY(bar->maximum() > 0);

  bar->setValue(bar->minimum());
  QWheelEvent up(QPointF(40, 40), QPointF(40, 40), QPoint(), QPoint(0, 120),
                 Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QVERIFY(QCoreApplication::sendEvent(scroll.viewport(), &up));
  QTRY_COMPARE_WITH_TIMEOUT(bar->value(), bar->minimum(), 500);

  bar->setValue(bar->maximum());
  QWheelEvent down(QPointF(40, 40), QPointF(40, 40), QPoint(), QPoint(0, -120),
                   Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QVERIFY(QCoreApplication::sendEvent(scroll.viewport(), &down));
  QTRY_COMPARE_WITH_TIMEOUT(bar->value(), bar->maximum(), 500);
}

void MainWindowTests::wheelOverComboScrollsTheSettingsPage() {
  const QString appName =
      QString("QtClickerMainWindowTest-%1").arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::make_unique<MainWindowFakeHotkeyService>(),
                    std::move(repository));
  window.resize(820, 560);
  window.show();
  QCoreApplication::processEvents();

  auto* pages = window.findChild<QStackedWidget*>("contentPages");
  auto* combo = window.findChild<QComboBox*>("targetModeCombo");
  QVERIFY(pages);
  QVERIFY(combo);
  auto* scroll = dynamic_cast<SmoothScrollArea*>(pages->currentWidget());
  QVERIFY(scroll);
  auto* bar = scroll->verticalScrollBar();
  QVERIFY(bar->maximum() > 0);
  bar->setValue(bar->minimum());
  const int selectedIndex = combo->currentIndex();

  QWheelEvent event(combo->rect().center(), combo->mapToGlobal(combo->rect().center()),
                    QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QVERIFY(QCoreApplication::sendEvent(combo, &event));
  QTRY_VERIFY_WITH_TIMEOUT(bar->value() > bar->minimum(), 500);
  QCOMPARE(combo->currentIndex(), selectedIndex);
}

void MainWindowTests::macroServicesRecordPersistAndReplay() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString appName =
      QString("QtClickerMacroWindowTest-%1").arg(QUuid::createUuid().toString());
  auto settings = std::make_unique<SettingsRepository>("OpenAI", appName);
  settings->setMacroSafetyAcknowledged(true);

  MacroPlatformServices macroServices;
  macroServices.windowService = std::make_unique<MainWindowFakeWindowService>();
  auto recorder = std::make_unique<MainWindowFakeMacroRecorder>();
  auto* observedRecorder = recorder.get();
  macroServices.recorder = std::move(recorder);
  auto player = std::make_unique<MainWindowFakeMacroPlayer>();
  auto* observedPlayer = player.get();
  macroServices.player = std::move(player);
  auto macros = std::make_unique<MacroRepository>(directory.path());

  MainWindow window(
      std::make_unique<MainWindowFakeClickBackend>(),
      std::make_unique<MainWindowFakeHotkeyService>(), std::move(settings),
      std::move(macroServices), std::move(macros),
      [](QWidget*) { return true; },
      [](QWidget*) { return QString("测试录制"); });

  auto* recordButton = window.findChild<QPushButton*>("macroRecordButton");
  auto* playButton = window.findChild<QPushButton*>("macroPlayButton");
  QVERIFY(recordButton);
  QVERIFY(playButton);
  recordButton->click();
  QVERIFY(observedRecorder->running);
  QCOMPARE(recordButton->text(), QString("停止录制"));

  MacroEvent down;
  down.type = MacroEventType::KeyDown;
  down.offsetUs = 1000;
  down.virtualKey = 'A';
  observedRecorder->capture(down);
  MacroEvent up = down;
  up.type = MacroEventType::KeyUp;
  up.offsetUs = 2000;
  observedRecorder->capture(up);
  recordButton->click();

  MacroRepository persisted(directory.path());
  const auto saved = persisted.loadAll();
  QCOMPARE(saved.size(), 1);
  QCOMPARE(saved.first().name, QString("测试录制"));
  QVERIFY(playButton->isEnabled());
  playButton->click();
  QTRY_COMPARE_WITH_TIMEOUT(observedPlayer->injected.size(), 2, 500);
  QCOMPARE(observedPlayer->prepared.id, saved.first().id);
}

void MainWindowTests::startsWithGlobalHotkeysDisabled() {
  const QString appName =
      QString("QtClickerHotkeyLifecycleTest-%1")
          .arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  auto hotkeys = std::make_unique<MainWindowFakeHotkeyService>();
  auto* observedHotkeys = hotkeys.get();
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::move(hotkeys), std::move(repository));

  auto* toggle =
      window.findChild<QCheckBox*>("globalHotkeysEnabledCheck");
  auto* startButton =
      window.findChild<QPushButton*>("startStopButton");
  QVERIFY(toggle);
  QVERIFY(startButton);
  QVERIFY(!toggle->isChecked());
  QCOMPARE(observedHotkeys->registerCount, 0);

  startButton->click();
  QCOMPARE(observedHotkeys->registerCount, 0);
}

void MainWindowTests::globalHotkeysRequireManualActivation() {
  const QString appName =
      QString("QtClickerHotkeyLifecycleTest-%1")
          .arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  auto hotkeys = std::make_unique<MainWindowFakeHotkeyService>();
  auto* observedHotkeys = hotkeys.get();
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::move(hotkeys), std::move(repository));

  auto* toggle =
      window.findChild<QCheckBox*>("globalHotkeysEnabledCheck");
  QVERIFY(toggle);
  toggle->click();
  QCOMPARE(observedHotkeys->registerCount, 1);
  QVERIFY(toggle->isChecked());

  toggle->click();
  QCOMPARE(observedHotkeys->unregisterCount, 1);
  QVERIFY(!toggle->isChecked());
}

void MainWindowTests::registrationFailureReturnsActivationToOff() {
  const QString appName =
      QString("QtClickerHotkeyLifecycleTest-%1")
          .arg(QUuid::createUuid().toString());
  auto repository = std::make_unique<SettingsRepository>("OpenAI", appName);
  auto hotkeys = std::make_unique<MainWindowFakeHotkeyService>();
  hotkeys->registrationResult = false;
  auto* observedHotkeys = hotkeys.get();
  MainWindow window(std::make_unique<MainWindowFakeClickBackend>(),
                    std::move(hotkeys), std::move(repository));

  auto* toggle =
      window.findChild<QCheckBox*>("globalHotkeysEnabledCheck");
  QVERIFY(toggle);
  toggle->click();
  QCOMPARE(observedHotkeys->registerCount, 1);
  QVERIFY(observedHotkeys->unregisterCount >= 1);
  QVERIFY(!toggle->isChecked());
}

QTEST_MAIN(MainWindowTests)

#include "MainWindowTests.moc"
