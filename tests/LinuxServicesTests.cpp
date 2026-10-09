#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>
#include "core/ClickBackend.h"
#include "core/HotkeyService.h"
#include "platform/PlatformServices.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

class LinuxServicesTests : public QObject {
  Q_OBJECT
 private slots:
  void fixedClickMovesCursor() {
    auto backend = createClickBackend();
    QVERIFY(backend->hasAccessibilityPermission());
    ClickProfile profile;
    profile.targetMode = TargetMode::FixedPoint;
    profile.fixedPoint = {100, 120};
    QVERIFY(backend->click(profile));
    QCOMPARE(backend->currentCursorPosition(), profile.fixedPoint);
  }
  void hotkeyConflictAndDispatch() {
    auto service = createHotkeyService();
    auto conflicting = createHotkeyService();
    ClickProfile profile;
    QVERIFY(service->registerHotkeys(profile));
    QVERIFY(!conflicting->registerHotkeys(profile));
    QSignalSpy pressed(service.get(), &HotkeyService::startStopPressed);
    Display* display = XOpenDisplay(nullptr);
    QVERIFY(display);
    const auto key = XKeysymToKeycode(display, XK_F6);
    XTestFakeKeyEvent(display, key, True, 0);
    XTestFakeKeyEvent(display, key, False, 0);
    XSync(display, False);
    XCloseDisplay(display);
    QTRY_COMPARE(pressed.count(), 1);
    service->unregisterAll();
    QVERIFY(conflicting->registerHotkeys(profile));
    conflicting->unregisterAll();
    profile.hotkeys.emergencyStop = profile.hotkeys.startStop;
    QVERIFY(!service->registerHotkeys(profile));
    QVERIFY(conflicting->registerHotkeys(ClickProfile{}));
  }
};
QTEST_MAIN(LinuxServicesTests)
#include "LinuxServicesTests.moc"
