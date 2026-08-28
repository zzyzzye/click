#include <QTest>

#include "platform/windows/WindowsWindowStyle.h"

class WindowsWindowStyleTests : public QObject {
  Q_OBJECT

 private slots:
  void windows11DetectedAtBuild22000();
  void windows10RejectedAtBuild19045();
  void boundaryBuild21999IsRejected();
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

QTEST_APPLESS_MAIN(WindowsWindowStyleTests)
#include "WindowsWindowStyleTests.moc"
