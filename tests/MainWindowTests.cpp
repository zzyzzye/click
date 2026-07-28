#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTest>
#include <QUuid>

#include <memory>

#include "app/MainWindow.h"
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"
#include "platform/PlatformServices.h"
#include "platform/windows/WindowsClickBackend.h"
#include "platform/windows/WindowsHotkeyService.h"
#include "app/widgets/ActionBar.h"
#include "app/widgets/NavigationSidebar.h"
#include "app/widgets/StatusStrip.h"

class MainWindowFakeClickBackend final : public ClickBackend {
 public:
  bool click(const ClickProfile& profile) override {
    lastProfile = profile;
    ++clickCount;
    return true;
  }

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

  bool registerHotkeys(const ClickProfile&) override {
    return true;
  }

  void unregisterAll() override {}

  QString backendName() const override {
    return "Fake";
  }
};

class MainWindowTests : public QObject {
  Q_OBJECT

 private slots:
  void windowsFactoriesCreateNativeServices();
  void availableInputHidesPermissionRequest();
  void loadedProfileRoundTripsThroughStart();
  void modeControlsFollowProfileChoices();
  void captureHotkeyUsesCurrentCursor();
  void usesPersistentClickFlowShell();
  void controlChevronResourcesAreAvailable();
  void usesClickFlowControlChrome();
};

void MainWindowTests::windowsFactoriesCreateNativeServices() {
  auto backend = createClickBackend();
  auto hotkeys = createHotkeyService();

  QVERIFY(dynamic_cast<WindowsClickBackend*>(backend.get()));
  QVERIFY(dynamic_cast<WindowsHotkeyService*>(hotkeys.get()));
}

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
  QCOMPARE(sidebar->pageCount(), 3);
  QCOMPARE(pages->count(), 3);
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

QTEST_MAIN(MainWindowTests)

#include "MainWindowTests.moc"
