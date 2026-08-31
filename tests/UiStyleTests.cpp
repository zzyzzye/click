#include <QTest>

#include "app/UiStyle.h"

class UiStyleTests : public QObject {
  Q_OBJECT

 private slots:
  void tokensAreFluentLight();
  void translucentSurfacesAreTransparent();
  void opaqueSurfacesUseFallback();
  void fluentControlMetrics();
  void controlChromePreserved();
};

void UiStyleTests::tokensAreFluentLight() {
  const ThemeTokens& tokens = fluentLightTokens();
  QCOMPARE(tokens.accent, QColor("#0067C0"));
  QCOMPARE(tokens.textPrimary, QColor("#1B1B1B"));
  QCOMPARE(tokens.danger, QColor("#C42B1C"));
  QCOMPARE(tokens.controlRadius, 4);
  QCOMPARE(tokens.cardRadius, 8);
  QCOMPARE(tokens.controlHeight, 32);
}

void UiStyleTests::translucentSurfacesAreTransparent() {
  const QString style = clickFlowStyleSheet(true).simplified();
  QVERIFY(style.contains("QMainWindow, #contentSurface { background: transparent"));
  QVERIFY(style.contains("#contentPages { background: transparent"));
  QVERIFY(style.contains("#navigationSidebar { background: transparent"));
}

void UiStyleTests::opaqueSurfacesUseFallback() {
  const QString style = clickFlowStyleSheet(false).simplified();
  QVERIFY(style.contains("QMainWindow, #contentSurface { background: #F3F3F3"));
  QVERIFY(style.contains("#contentPages { background: #F3F3F3"));
  QVERIFY(style.contains("#navigationSidebar { background: #F3F3F3"));
  // 回退模式下卡片保持纯白
  QVERIFY(style.contains("#settingsCard, #statusStrip, #actionBar { background: white"));
}

void UiStyleTests::fluentControlMetrics() {
  const QString style = clickFlowStyleSheet(true);
  QVERIFY(style.contains("min-height: 32px"));
  QVERIFY(style.contains("border-radius: 4px"));
  QVERIFY(style.contains("#0067C0"));
  QVERIFY(style.contains("#C42B1C"));
}

void UiStyleTests::controlChromePreserved() {
  const QString style = clickFlowStyleSheet(true);
  QVERIFY(style.contains("QComboBox::down-arrow"));
  QVERIFY(style.contains("QSpinBox::up-button"));
  QVERIFY(style.contains("QSpinBox::down-button"));
  QVERIFY(style.contains(":/clickflow/icons/chevron-down.svg"));
  QVERIFY(style.contains(":/clickflow/icons/chevron-up.svg"));
  QVERIFY(style.contains(":/clickflow/icons/check.svg"));
}

QTEST_APPLESS_MAIN(UiStyleTests)
#include "UiStyleTests.moc"
