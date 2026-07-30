#include <QTest>
#include <QKeySequenceEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

#include "app/pages/ClickSettingsPage.h"
#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/MacroRecordingPage.h"
#include "app/pages/PresetsAboutPage.h"

class ClickFlowPageTests : public QObject {
  Q_OBJECT

 private slots:
  void clickSettingsRoundTrip();
  void hotkeysRoundTripAndValidate();
  void presetsAndAboutExposeProductState();
  void macroPageKeepsHotkeysVisibleAndEmitsSettings();
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
  input.hotkeys.macroRecord = "Ctrl+F9";
  input.hotkeys.macroPlayback = "Ctrl+F10";

  HotkeySettingsPage page;
  page.setProfile(input);
  ClickProfile output;
  page.applyToProfile(output);
  QCOMPARE(output.hotkeys.startStop, QString("Ctrl+F6"));
  QCOMPARE(output.hotkeys.capturePoint, QString("Ctrl+F7"));
  QCOMPARE(output.hotkeys.emergencyStop, QString("Ctrl+F8"));
  QCOMPARE(output.hotkeys.macroRecord, QString("Ctrl+F9"));
  QCOMPARE(output.hotkeys.macroPlayback, QString("Ctrl+F10"));
  QVERIFY(page.findChild<QKeySequenceEdit*>("macroRecordHotkeyEdit"));
  QVERIFY(page.findChild<QKeySequenceEdit*>("macroPlaybackHotkeyEdit"));
  QString error;
  QVERIFY(page.validate(input, &error));
  input.inputMode = InputMode::Keyboard;
  input.keyboardKey = "F6";
  QVERIFY(!page.validate(input, &error));
  QVERIFY(error.contains("冲突"));

  input.inputMode = InputMode::Mouse;
  input.hotkeys.macroRecord = input.hotkeys.startStop;
  page.setProfile(input);
  QVERIFY(!page.validate(input, &error));
  QVERIFY(error.contains("重复"));
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

void ClickFlowPageTests::macroPageKeepsHotkeysVisibleAndEmitsSettings() {
  MacroRecordingPage page;
  HotkeyBindings hotkeys;
  hotkeys.macroRecord = "Ctrl+F9";
  hotkeys.macroPlayback = "Ctrl+F10";
  hotkeys.emergencyStop = "Ctrl+F8";
  page.setHotkeys(hotkeys, true);

  auto* recordHotkey = page.findChild<QLabel*>("macroRecordHotkeyLabel");
  auto* playbackHotkey = page.findChild<QLabel*>("macroPlaybackHotkeyLabel");
  auto* emergencyHotkey = page.findChild<QLabel*>("macroEmergencyHotkeyLabel");
  QVERIFY(recordHotkey);
  QVERIFY(playbackHotkey);
  QVERIFY(emergencyHotkey);
  QVERIFY(recordHotkey->text().contains("Ctrl+F9"));
  QVERIFY(playbackHotkey->text().contains("Ctrl+F10"));
  QVERIFY(emergencyHotkey->text().contains("Ctrl+F8"));

  WindowTarget target;
  target.nativeId = 42;
  target.title = "目标窗口";
  target.executablePath = "C:/Apps/target.exe";
  target.className = "TargetWindow";
  target.clientSize = QSize(800, 600);
  page.setAvailableWindows({target});
  auto* targetMode = page.findChild<QComboBox*>("macroTargetModeCombo");
  auto* windowCombo = page.findChild<QComboBox*>("macroWindowCombo");
  QVERIFY(targetMode);
  QVERIFY(windowCombo);
  targetMode->setCurrentIndex(
      targetMode->findData(static_cast<int>(MacroTargetMode::Window)));
  QVERIFY(windowCombo->isEnabled());

  QSignalSpy recordSpy(&page, &MacroRecordingPage::recordRequested);
  auto* recordButton = page.findChild<QPushButton*>("macroRecordButton");
  QVERIFY(recordButton);
  recordButton->click();
  QCOMPARE(recordSpy.count(), 1);
  const auto options =
      qvariant_cast<MacroRecordingOptions>(recordSpy.first().first());
  QCOMPARE(options.targetMode, MacroTargetMode::Window);
  QCOMPARE(options.target.nativeId, quintptr(42));
  QCOMPARE(options.reservedHotkeys,
           QStringList({"Ctrl+F9", "Ctrl+F10", "Ctrl+F8"}));

  MacroSequence sequence;
  sequence.id = "macro-1";
  sequence.name = "演示宏";
  sequence.createdAt = QDateTime::currentDateTimeUtc();
  sequence.modifiedAt = sequence.createdAt;
  sequence.durationUs = 1000;
  MacroEvent event;
  event.type = MacroEventType::KeyDown;
  sequence.events.append(event);
  page.setMacros({sequence}, sequence.id);
  QSignalSpy playSpy(&page, &MacroRecordingPage::playRequested);
  auto* playButton = page.findChild<QPushButton*>("macroPlayButton");
  QVERIFY(playButton);
  playButton->click();
  QCOMPARE(playSpy.count(), 1);
  QCOMPARE(playSpy.first().at(0).toString(), sequence.id);

  page.setActivity(MacroPageActivity::Recording);
  QVERIFY(recordHotkey->isVisibleTo(&page));
  QVERIFY(playbackHotkey->isVisibleTo(&page));
  QVERIFY(emergencyHotkey->isVisibleTo(&page));
  QCOMPARE(recordButton->text(), QString("停止录制"));
  QVERIFY(!playButton->isEnabled());
}

QTEST_MAIN(ClickFlowPageTests)

#include "ClickFlowPageTests.moc"
