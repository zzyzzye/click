#include <QTest>

#include "platform/windows/WindowsWindowStyle.h"

class WindowsWindowStyleTests : public QObject {
  Q_OBJECT

 private slots:
  void windows11DetectedAtBuild22000();
  void windows10RejectedAtBuild19045();
  void boundaryBuild21999IsRejected();
  void systemBackdropRequiresBuild22621();
  void micaBackdropRequiresTransparency();
  void windows11ThresholdUnchanged();
};

void WindowsWindowStyleTests::windows11DetectedAtBuild22000() {
  QVERIFY(isWindows11OrLater(22000));
}

void WindowsWindowStyleTests::windows10RejectedAtBuild19045() {
  QVERIFY(!isWindows11OrLater(19045));
}

void WindowsWindowStyleTests::boundaryBuild21999IsRejected() {
  QVERIFY(!isWindows11OrLater(21999));
}

void WindowsWindowStyleTests::systemBackdropRequiresBuild22621() {
  QVERIFY(supportsSystemBackdrop(22621));
  QVERIFY(supportsSystemBackdrop(26200));
  QVERIFY(!supportsSystemBackdrop(22620));
  QVERIFY(!supportsSystemBackdrop(22000));
}

void WindowsWindowStyleTests::micaBackdropRequiresTransparency() {
  // Mica 需要：Win11 + build>=22621 + 系统「透明效果」开启，三者缺一不可
  QVERIFY(micaBackdropAvailable(26200, true));
  QVERIFY(!micaBackdropAvailable(26200, false));
  QVERIFY(!micaBackdropAvailable(22620, true));
  QVERIFY(!micaBackdropAvailable(19045, true));
}

void WindowsWindowStyleTests::windows11ThresholdUnchanged() {
  QVERIFY(isWindows11OrLater(22000));
  QVERIFY(!isWindows11OrLater(21999));
}

QTEST_APPLESS_MAIN(WindowsWindowStyleTests)
#include "WindowsWindowStyleTests.moc"
