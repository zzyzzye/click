#include <QTest>

#include "app/controllers/UpdateController.h"

class UpdateControllerTests : public QObject {
  Q_OBJECT

 private slots:
  void detectsNewerRelease();
  void rejectsSameOrOlderRelease();
  void acceptsOptionalVersionPrefix();
};

void UpdateControllerTests::detectsNewerRelease() {
  QVERIFY(isReleaseNewer("0.5.1", "0.5.0"));
  QVERIFY(isReleaseNewer("0.6.0", "0.5.9"));
  QVERIFY(isReleaseNewer("1.0.0", "0.9.9"));
}

void UpdateControllerTests::rejectsSameOrOlderRelease() {
  QVERIFY(!isReleaseNewer("0.5.0", "0.5.0"));
  QVERIFY(!isReleaseNewer("0.4.9", "0.5.0"));
  QVERIFY(!isReleaseNewer("0.5.0", "0.5.1"));
}

void UpdateControllerTests::acceptsOptionalVersionPrefix() {
  QVERIFY(isReleaseNewer("v0.5.1", "0.5.0"));
  QVERIFY(!isReleaseNewer("v0.5.0", "0.5.0"));
}

QTEST_APPLESS_MAIN(UpdateControllerTests)
#include "UpdateControllerTests.moc"
