#include <QTest>

#include "app/pages/ClickSettingsPage.h"
#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/PresetsAboutPage.h"

class ClickFlowPageTests : public QObject {
  Q_OBJECT

 private slots:
  void clickSettingsRoundTrip();
  void hotkeysRoundTripAndValidate();
  void presetsAndAboutExposeProductState();
};

void ClickFlowPageTests::clickSettingsRoundTrip() {
  ClickProfile input;
  input.intervalMs = 250;
  input.inputMode = InputMode::Keyboard;
  input.keyboardKey = "Space";
  input.button = ClickButton::Right;
  input.targetMode = TargetMode::FixedPoint;
  input.fixedPoint = QPoint(640, 480);
  input.repeatMode = RepeatMode::Finite;
  input.repeatCount = 8;
  input.jitterRadius = 7;
  input.countdownSeconds = 3;
  input.alwaysOnTop = true;

  ClickSettingsPage page;
  page.setProfile(input);
  ClickProfile output;
  page.applyToProfile(output);

  QCOMPARE(output.intervalMs, 250);
  QCOMPARE(output.inputMode, InputMode::Keyboard);
  QCOMPARE(output.keyboardKey, QString("Space"));
  QCOMPARE(output.button, ClickButton::Right);
  QCOMPARE(output.targetMode, TargetMode::FixedPoint);
  QCOMPARE(output.fixedPoint, QPoint(640, 480));
  QCOMPARE(output.repeatMode, RepeatMode::Finite);
  QCOMPARE(output.repeatCount, 8);
  QCOMPARE(output.jitterRadius, 7);
  QCOMPARE(output.countdownSeconds, 3);
  QCOMPARE(output.alwaysOnTop, true);
  QVERIFY(!page.fixedControlsEnabled());
  QVERIFY(page.repeatCountEnabled());
}

void ClickFlowPageTests::hotkeysRoundTripAndValidate() {
  ClickProfile input;
  input.hotkeys.startStop = "Ctrl+F6";
  input.hotkeys.capturePoint = "Ctrl+F7";
  input.hotkeys.emergencyStop = "Ctrl+F8";

  HotkeySettingsPage page;
  page.setProfile(input);
  ClickProfile output;
  page.applyToProfile(output);
  QCOMPARE(output.hotkeys.startStop, QString("Ctrl+F6"));
  QCOMPARE(output.hotkeys.capturePoint, QString("Ctrl+F7"));
  QCOMPARE(output.hotkeys.emergencyStop, QString("Ctrl+F8"));
  QString error;
  QVERIFY(page.validate(input, &error));
  input.inputMode = InputMode::Keyboard;
  input.keyboardKey = "F6";
  QVERIFY(!page.validate(input, &error));
  QVERIFY(error.contains("冲突"));
}

void ClickFlowPageTests::presetsAndAboutExposeProductState() {
  QCoreApplication::setApplicationVersion("0.2.0");
  PresetsAboutPage page;
  page.setPresetNames({"Alpha", "Beta"}, "Beta");

  QCOMPARE(page.selectedPresetName(), QString("Beta"));
  QCOMPARE(page.productName(), QString("ClickFlow"));
  QCOMPARE(page.versionText(), QString("0.2.0"));
  QVERIFY(!page.platformText().isEmpty());
  QVERIFY(!page.qtVersionText().isEmpty());
}

QTEST_MAIN(ClickFlowPageTests)

#include "ClickFlowPageTests.moc"
