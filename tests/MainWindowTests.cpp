#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QUuid>

#include <memory>

#include "app/MainWindow.h"
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"
#include "platform/PlatformServices.h"
#include "platform/windows/WindowsClickBackend.h"
#include "platform/windows/WindowsHotkeyService.h"

class MainWindowFakeClickBackend final : public ClickBackend {
 public:
  bool click(const ClickProfile&) override {
    return true;
  }

  QPoint currentCursorPosition() const override {
    return QPoint(25, 35);
  }

  bool hasAccessibilityPermission() const override {
    return true;
  }

  void requestAccessibilityPermission() override {}
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

QTEST_MAIN(MainWindowTests)

#include "MainWindowTests.moc"
