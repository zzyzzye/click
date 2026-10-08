#include <QGuiApplication>
#include <QTest>

#include "ClickFlowVersion.h"
#include "app/AppIdentity.h"

class AppIdentityTests : public QObject {
  Q_OBJECT

 private slots:
  void appliesClickFlowIdentity();
};

void AppIdentityTests::appliesClickFlowIdentity() {
  applyApplicationIdentity();

  QCOMPARE(QCoreApplication::organizationName(), QString("ClickFlow"));
  QCOMPARE(QCoreApplication::applicationName(), QString("ClickFlow"));
  QCOMPARE(QCoreApplication::applicationVersion(), QString("0.5.1"));
  QCOMPARE(QGuiApplication::applicationDisplayName(), QString("ClickFlow"));
  QCOMPARE(QString(ClickFlowVersion::string), QString("0.5.1"));
}

QTEST_MAIN(AppIdentityTests)

#include "AppIdentityTests.moc"
