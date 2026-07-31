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

  QCOMPARE(QCoreApplication::organizationName(), QString("OpenAI"));
  QCOMPARE(QCoreApplication::applicationName(), QString("QtClicker"));
  QCOMPARE(QCoreApplication::applicationVersion(), QString("0.4.0"));
  QCOMPARE(QGuiApplication::applicationDisplayName(), QString("ClickFlow"));
  QCOMPARE(QString(ClickFlowVersion::string), QString("0.4.0"));
}

QTEST_MAIN(AppIdentityTests)

#include "AppIdentityTests.moc"
