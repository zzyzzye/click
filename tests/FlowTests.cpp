#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include "core/FlowDocument.h"
#include "core/FlowRepository.h"
#include "core/FlowPlayer.h"
#include "core/MacroController.h"
#include "core/MacroRepository.h"
#include "app/pages/FlowEditorPage.h"
#include "app/controllers/FlowWorkflowController.h"

class FlowBackend final : public ClickBackend {
public:
  bool click(const ClickProfile& p) override {
    points.append(p.fixedPoint);
    buttons.append(p.button);
    times.append(clock.elapsed());
    return success;
  }
  bool keyTap(const ClickProfile& p) override {
    keys.append(p.keyboardKey);
    return success;
  }
  bool textInput(const QString& s) override {
    text += s;
    return success;
  }
  bool supportsText() const override {
    return textSupported;
  }
  bool supportsKey(const QString& s) const override {
    return QKeySequence::fromString(s).count() == 1 && s != "Unknown";
  }
  QPoint currentCursorPosition() const override {
    return {25, 35};
  }
  bool hasAccessibilityPermission() const override {
    return permission;
  }
  void requestAccessibilityPermission() override {}
  QVector<QRect> screenRects() const override {
    return screens;
  }
  QVector<QRect> screens{QRect(-100, 0, 200, 200), QRect(200, 0, 200, 200)};
  QVector<QPoint> points;
  QVector<ClickButton> buttons;
  QVector<qint64> times;
  QStringList keys;
  QString text;
  QElapsedTimer clock;
  bool permission = true, success = true, textSupported = true;
};
class RawFlowPlayer final : public MacroPlayer {
public:
  bool prepare(const MacroSequence& s, QString*) override {
    prepared = s;
    return true;
  }
  bool inject(const MacroEvent& e, QString*) override {
    events.append(e);
    if (e.type == MacroEventType::KeyUp && failUp)
      return false;
    if (e.type == MacroEventType::KeyDown)
      held = true;
    if (e.type == MacroEventType::KeyUp)
      held = false;
    return true;
  }
  bool revalidate(QString* error) override {
    if (!valid && error)
      *error = "目标窗口失效";
    return valid;
  }
  void releaseAll() override {
    ++releases;
    held = false;
  }
  void cancel() override {
    ++cancels;
    releaseAll();
  }
  QVector<MacroEvent> events;
  MacroSequence prepared;
  bool valid = true;
  bool held = false;
  bool failUp = false;
  int releases = 0, cancels = 0;
};
FlowDocument clickFlow(const FlowBackend& b) {
  auto d = FlowTypes::create();
  d.screens = b.screens;
  auto s = FlowTypes::createStep(FlowStepKind::Click);
  s.point = {10, 20};
  s.waitMs = 20;
  d.steps.append(s);
  return d;
}
MacroSequence rawRecording() {
  MacroSequence s;
  s.id = "old-recording";
  s.name = "旧录制";
  s.createdAt = s.modifiedAt = QDateTime::currentDateTimeUtc();
  s.durationUs = 120000;
  MacroEvent down;
  down.type = MacroEventType::KeyDown;
  down.virtualKey = 65;
  MacroEvent up = down;
  up.type = MacroEventType::KeyUp;
  up.offsetUs = 120000;
  s.events = {down, up};
  return s;
}
class FlowTests final : public QObject {
  Q_OBJECT
private slots:
  void persistenceAndCompatibility();
  void rejectsInvalidDocuments();
  void executesThreePointsAndRounds();
  void countsButtonsAndRestart();
  void lateDispatchDoesNotShortenManualWaits();
  void pausesWaitingAndKeepsRemainingTime();
  void pausesOnlyAfterRawBlock();
  void pausedOwnershipAndEmergencyStop();
  void resumeRevalidatesConditions();
  void invalidPointsAndCapabilitiesFailBeforeInput();
  void editorHistoryAndSaveOpen();
  void trialDoesNotModifySavedFlow();
  void saveFailureAndUnsavedCancel();
  void stopFromProgressAndInjectionFailure();
  void captureCancellationAndHotkeyConflict();
};
void FlowTests::persistenceAndCompatibility() {
  QTemporaryDir tmp;
  FlowBackend b;
  auto d = clickFlow(b);
  auto key = FlowTypes::createStep(FlowStepKind::Key);
  key.value = "Ctrl+A";
  d.steps.append(key);
  auto text = FlowTypes::createStep(FlowStepKind::Text);
  text.value = QString::fromUtf8("测试😀");
  d.steps.append(text);
  auto raw = FlowTypes::createStep(FlowStepKind::Recording);
  raw.recording = rawRecording();
  raw.recording.playback.speed = 2;
  d.steps.append(raw);
  d.playback.repeatCount = 3;
  d.playback.loopDelayMs = 35;
  FlowRepository repo(tmp.path() + "/flows");
  QString error;
  QVERIFY2(repo.save(d, &error), qPrintable(error));
  const auto loaded = repo.loadAll();
  QCOMPARE(loaded.size(), 1);
  QCOMPARE(FlowTypes::toJson(loaded[0]), FlowTypes::toJson(d));
  MacroRepository old(tmp.path() + "/macros");
  QVERIFY(old.save(raw.recording));
  QFile file(tmp.path() + "/macros/old-recording.json");
  QVERIFY(file.open(QIODevice::ReadOnly));
  const auto original = file.readAll();
  file.close();
  auto compiled = FlowTypes::compile(loaded[0]);
  QVERIFY(compiled);
  const auto start = compiled->events[compiled->events.size() - 3].offsetUs;
  QCOMPARE(compiled->events[compiled->events.size() - 2].offsetUs - start, qint64(60000));
  QVERIFY(repo.save(loaded[0]));
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), original);
  auto json = FlowTypes::toJson(d);
  json.insert("version", 99);
  QVERIFY(!FlowTypes::fromJson(json, &error));
  QVERIFY(error.contains("版本"));
}
void FlowTests::rejectsInvalidDocuments() {
  auto d = FlowTypes::create();
  QString error;
  QVERIFY(!FlowTypes::compile(d, &error));
  d.steps.append(FlowTypes::createStep(FlowStepKind::Click));
  d.steps[0].count = 0;
  int index = -1;
  QVERIFY(!FlowTypes::validate(d, &error, &index));
  QCOMPARE(index, 0);
  d.steps[0].count = 1;
  d.steps[0].waitMs = -1;
  QVERIFY(!FlowTypes::validate(d));
  d.steps[0].waitMs = 10;
  d.steps.append(d.steps[0]);
  QVERIFY(!FlowTypes::validate(d));
  d.steps.removeLast();
  d.steps[0].kind = FlowStepKind(99);
  QVERIFY(!FlowTypes::validate(d));
  d.steps[0].kind = FlowStepKind::Text;
  d.steps[0].value.clear();
  QVERIFY(!FlowTypes::validate(d));
  d.steps[0].value = "text";
  d.id = "../escape";
  QVERIFY(!FlowTypes::validate(d));
  d = FlowTypes::create();
  d.steps.append(FlowTypes::createStep(FlowStepKind::Click));
  auto json = FlowTypes::toJson(d);
  auto steps = json["steps"].toArray();
  auto item = steps[0].toObject();
  item.insert("x", "not-a-coordinate");
  steps[0] = item;
  json.insert("steps", steps);
  QVERIFY(!FlowTypes::fromJson(json, &error));
  QVERIFY(error.contains("步骤 1"));
  auto raw = MacroTypes::toJson(rawRecording());
  raw.insert("durationUs", 1e100);
  QVERIFY(!MacroTypes::fromJson(raw, &error));
}
void FlowTests::executesThreePointsAndRounds() {
  FlowBackend b;
  b.clock.start();
  auto d = clickFlow(b);
  for (int i = 1; i < 3; ++i) {
    auto s = FlowTypes::createStep(FlowStepKind::Click);
    s.point = {10 + i, 20};
    s.waitMs = 30 + i * 10;
    d.steps.append(s);
  }
  d.playback.repeatCount = 2;
  d.playback.loopDelayMs = 30;
  AutomationCoordinator coordinator;
  FlowPlayer player(&b, nullptr);
  player.setDocument(d);
  MacroController controller(nullptr, &player, &coordinator);
  QSignalSpy progress(&controller, &MacroController::stepProgress);
  QVERIFY(controller.startPlayback(*FlowTypes::compile(d)));
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(), MacroControllerState::Idle, 1500);
  QCOMPARE(b.points, QVector<QPoint>({{10, 20}, {11, 20}, {12, 20}, {10, 20}, {11, 20}, {12, 20}}));
  QVERIFY(b.times[1] - b.times[0] >= 25);
  QVERIFY(b.times[2] - b.times[1] >= 45);
  QVERIFY(b.times[3] - b.times[2] >= 85);
  QCOMPARE(progress.last()[2].toInt(), 2);
}
void FlowTests::countsButtonsAndRestart() {
  FlowBackend b;
  b.clock.start();
  auto d = clickFlow(b);
  d.steps[0].count = 2;
  auto s = FlowTypes::createStep(FlowStepKind::Click);
  s.point = {30, 40};
  s.button = MacroMouseButton::Right;
  s.count = 3;
  s.waitMs = 0;
  d.steps.append(s);
  AutomationCoordinator c;
  RawFlowPlayer raw;
  FlowPlayer p(&b, &raw);
  p.setDocument(d);
  MacroController runner(nullptr, &p, &c);
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  QTRY_COMPARE_WITH_TIMEOUT(runner.state(), MacroControllerState::Idle, 500);
  QCOMPARE(b.buttons,
           QVector<ClickButton>({ClickButton::Left, ClickButton::Left, ClickButton::Right,
                                 ClickButton::Right, ClickButton::Right}));
  QCOMPARE(raw.cancels, 0);
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  runner.stop();
  QCOMPARE(b.points.last(), QPoint(10, 20));
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  runner.stop();
  QCOMPARE(b.points.last(), QPoint(10, 20));
}
void FlowTests::lateDispatchDoesNotShortenManualWaits() {
  FlowBackend b;
  b.clock.start();
  auto d = clickFlow(b);
  d.steps[0].waitMs = 20;
  auto s = d.steps[0];
  s.id = "second";
  s.point = {30, 40};
  s.waitMs = 100;
  d.steps.append(s);
  s.id = "third";
  s.point = {40, 50};
  s.waitMs = 0;
  d.steps.append(s);
  AutomationCoordinator c;
  FlowPlayer p(&b, nullptr);
  p.setDocument(d);
  MacroController runner(nullptr, &p, &c);
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  // 故意阻塞测试事件循环，模拟系统繁忙；第二、三点仍须保留独立等待。
  QTest::qSleep(160);
  QCoreApplication::processEvents();
  QTRY_COMPARE_WITH_TIMEOUT(b.points.size(), 3, 700);
  QVERIFY(b.times[2] - b.times[1] >= 105);
}
void FlowTests::pausesWaitingAndKeepsRemainingTime() {
  FlowBackend b;
  b.clock.start();
  auto d = clickFlow(b);
  d.steps[0].waitMs = 250;
  auto second = d.steps[0];
  second.id = "second";
  second.point = {30, 40};
  second.waitMs = 0;
  d.steps.append(second);
  AutomationCoordinator c;
  FlowPlayer p(&b, nullptr);
  p.setDocument(d);
  MacroController runner(nullptr, &p, &c);
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  QTest::qWait(65);
  runner.pause();
  QVERIFY(runner.isPaused());
  QTest::qWait(280);
  QCOMPARE(b.points.size(), 1);
  QElapsedTimer resumed;
  resumed.start();
  QVERIFY(runner.resume());
  QTest::qWait(65);
  QCOMPARE(b.points.size(), 1);
  QTRY_COMPARE_WITH_TIMEOUT(b.points.size(), 2, 500);
  QVERIFY(resumed.elapsed() >= 160);
}
void FlowTests::pausesOnlyAfterRawBlock() {
  FlowBackend b;
  RawFlowPlayer raw;
  auto d = FlowTypes::create();
  auto s = FlowTypes::createStep(FlowStepKind::Recording);
  s.recording = rawRecording();
  d.steps.append(s);
  auto click = FlowTypes::createStep(FlowStepKind::Click);
  click.point = {10, 20};
  d.screens = b.screens;
  d.steps.append(click);
  AutomationCoordinator c;
  FlowPlayer p(&b, &raw);
  p.setDocument(d);
  MacroController runner(nullptr, &p, &c);
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  QCOMPARE(raw.events.size(), 1);
  runner.pause();
  QCOMPARE(runner.state(), MacroControllerState::Playing);
  QTRY_VERIFY_WITH_TIMEOUT(runner.isPaused(), 500);
  QCOMPARE(raw.events.size(), 2);
  QVERIFY(!raw.held);
  QCOMPARE(b.points.size(), 0);
  QVERIFY(raw.releases > 0);
  QVERIFY(runner.resume());
  QTRY_COMPARE_WITH_TIMEOUT(b.points.size(), 1, 500);
  runner.emergencyStop();
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  QVERIFY(raw.held);
  runner.emergencyStop();
  QVERIFY(!raw.held);
  raw.failUp = true;
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  QTRY_COMPARE_WITH_TIMEOUT(runner.state(), MacroControllerState::Idle, 500);
  QVERIFY(!raw.held);
  QVERIFY(raw.cancels > 0);
}
void FlowTests::pausedOwnershipAndEmergencyStop() {
  FlowBackend b;
  auto d = clickFlow(b);
  d.playback.infinite = true;
  AutomationCoordinator c;
  FlowPlayer p(&b, nullptr);
  p.setDocument(d);
  MacroController runner(nullptr, &p, &c);
  connect(&c, &AutomationCoordinator::emergencyStopRequested, &runner,
          &MacroController::emergencyStop);
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  runner.pause();
  QVERIFY(runner.isPaused());
  QVERIFY(!c.tryAcquire(AutomationActivity::Clicking));
  c.requestEmergencyStop();
  QCOMPARE(runner.state(), MacroControllerState::Idle);
  QCOMPARE(c.activity(), AutomationActivity::Idle);
  const auto count = b.points.size();
  QTest::qWait(80);
  QCOMPARE(b.points.size(), count);
}
void FlowTests::resumeRevalidatesConditions() {
  FlowBackend b;
  auto d = clickFlow(b);
  AutomationCoordinator c;
  FlowPlayer p(&b, nullptr);
  p.setDocument(d);
  MacroController runner(nullptr, &p, &c);
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  runner.pause();
  b.screens[0].setWidth(300);
  QString error;
  QVERIFY(!runner.resume(&error));
  QVERIFY(error.contains("布局"));
  QCOMPARE(runner.state(), MacroControllerState::Idle);
  QCOMPARE(b.points.size(), 1);
  b.screens = d.screens;
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  runner.pause();
  b.permission = false;
  QVERIFY(!runner.resume(&error));
  QVERIFY(error.contains("权限"));
}
void FlowTests::invalidPointsAndCapabilitiesFailBeforeInput() {
  FlowBackend b;
  auto d = clickFlow(b);
  d.steps[0].point = {150, 20};
  AutomationCoordinator c;
  FlowPlayer p(&b, nullptr);
  p.setDocument(d);
  MacroController runner(nullptr, &p, &c);
  QString error;
  QVERIFY(!runner.startPlayback(*FlowTypes::compile(d), &error));
  QVERIFY(error.contains("步骤 1"));
  QVERIFY(b.points.isEmpty());
  d.steps[0].point = {10, 20};
  d.steps.append(FlowTypes::createStep(FlowStepKind::Text));
  d.steps[1].value = "文字";
  b.textSupported = false;
  p.setDocument(d);
  QVERIFY(!runner.startPlayback(*FlowTypes::compile(d), &error));
  QVERIFY(error.contains("步骤 2"));
  QVERIFY(b.points.isEmpty());
  auto raw = FlowTypes::createStep(FlowStepKind::Recording);
  raw.recording = rawRecording();
  d.steps = {raw};
  p.setDocument(d);
  QVERIFY(!runner.startPlayback(*FlowTypes::compile(d), &error));
  QVERIFY(error.contains("Windows"));
}
void FlowTests::editorHistoryAndSaveOpen() {
  FlowBackend b;
  FlowEditorPage page;
  QTemporaryDir tmp;
  AutomationCoordinator c;
  FlowWorkflowController workflow(&page, &b, nullptr, &c, tmp.path(), [] { return true; });
  auto click = [&](const char* id) { page.findChild<QPushButton*>(id)->click(); };
  auto* kind = page.findChild<QComboBox*>("flowStepKind");
  click("flowAdd");
  page.capturePoint({10, 20}, b.screens);
  click("flowCopy");
  QCOMPARE(page.document().steps.size(), 2);
  QVERIFY(page.document().steps[0].id != page.document().steps[1].id);
  page.findChild<QSpinBox*>("flowX")->setValue(30);
  click("flowUp");
  QCOMPARE(page.document().steps[0].point.x(), 30);
  click("flowUndo");
  QCOMPARE(page.document().steps[0].point.x(), 10);
  click("flowRedo");
  QCOMPARE(page.document().steps[0].point.x(), 30);
  click("flowRemove");
  QCOMPARE(page.document().steps.size(), 1);
  kind->setCurrentIndex(2);
  click("flowAdd");
  page.findChild<QPlainTextEdit*>("flowText")->setPlainText("中文😀");
  kind->setCurrentIndex(1);
  click("flowAdd");
  auto* key = page.findChild<QKeySequenceEdit*>("flowKey");
  key->setKeySequence(QKeySequence("Return"));
  emit key->editingFinished();
  kind->setCurrentIndex(3);
  click("flowAdd");
  page.findChild<QSpinBox*>("flowWait")->setValue(250);
  kind->setCurrentIndex(0);
  click("flowAdd");
  page.capturePoint({35, 40}, b.screens);
  click("flowSave");
  QVERIFY(!page.isDirty());
  FlowRepository repo(tmp.path());
  auto docs = repo.loadAll();
  QCOMPARE(docs.size(), 1);
  QCOMPARE(FlowTypes::toJson(docs[0]), FlowTypes::toJson(page.document()));
  page.setDocument(FlowTypes::create());
  auto* library = page.findChild<QComboBox*>("flowLibrary");
  library->setCurrentIndex(1);
  click("flowOpen");
  QCOMPARE(FlowTypes::toJson(page.document()), FlowTypes::toJson(docs[0]));
}
void FlowTests::trialDoesNotModifySavedFlow() {
  FlowBackend b;
  auto d = clickFlow(b);
  auto text = FlowTypes::createStep(FlowStepKind::Text);
  text.value = "中😀";
  d.steps.append(text);
  auto key = FlowTypes::createStep(FlowStepKind::Key);
  key.value = "Ctrl+A";
  d.steps.append(key);
  d.playback.infinite = true;
  FlowEditorPage page;
  page.setDocument(d);
  QTemporaryDir tmp;
  AutomationCoordinator c;
  FlowWorkflowController workflow(&page, &b, nullptr, &c, tmp.path(), [] { return true; });
  emit page.saveRequested();
  QFile f(QDir(tmp.path()).filePath(d.id + ".json"));
  QVERIFY(f.open(QIODevice::ReadOnly));
  auto data = f.readAll();
  f.close();
  emit page.runRequested(true);
  QTRY_VERIFY_WITH_TIMEOUT(!workflow.isRunning(), 500);
  QCOMPARE(b.points.size(), 1);
  QCOMPARE(b.text, text.value);
  QCOMPARE(b.keys, QStringList{"Ctrl+A"});
  QVERIFY(f.open(QIODevice::ReadOnly));
  QCOMPARE(f.readAll(), data);
  QVERIFY(!page.isDirty());
}
void FlowTests::saveFailureAndUnsavedCancel() {
  QTemporaryDir tmp;
  QFile block(tmp.path() + "/blocked");
  QVERIFY(block.open(QIODevice::WriteOnly));
  block.close();
  FlowBackend b;
  FlowEditorPage page;
  page.setDocument(clickFlow(b));
  page.findChild<QSpinBox*>("flowWait")->setValue(50);
  AutomationCoordinator c;
  FlowWorkflowController workflow(&page, &b, nullptr, &c, block.fileName(), [] { return true; });
  emit page.saveRequested();
  QVERIFY(page.isDirty());
  QVERIFY(page.findChild<QLabel*>("flowMessage")->text().contains("无法"));
  QTimer::singleShot(0, [] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    QVERIFY(box);
    box->done(QMessageBox::Cancel);
  });
  QVERIFY(!workflow.resolveUnsaved());
  QVERIFY(page.isDirty());
  QVERIFY(block.remove());
  QVERIFY(QDir().mkpath(block.fileName()));
  emit page.saveRequested();
  QVERIFY(!page.isDirty());
  page.findChild<QSpinBox*>("flowWait")->setValue(75);
  QTimer::singleShot(0, [] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    QVERIFY(box);
    box->done(QMessageBox::Save);
  });
  emit page.newRequested();
  QVERIFY(page.document().steps.isEmpty());
  FlowRepository repo(block.fileName());
  auto saved = repo.loadAll();
  QCOMPARE(saved.size(), 1);
  QCOMPARE(saved[0].steps[0].waitMs, 75);
  page.findChild<QPushButton*>("flowAdd")->click();
  QVERIFY(page.isDirty());
  QTimer::singleShot(0, [] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    QVERIFY(box);
    box->done(QMessageBox::Discard);
  });
  emit page.newRequested();
  QVERIFY(page.document().steps.isEmpty());
  QVERIFY(!page.isDirty());
}
void FlowTests::stopFromProgressAndInjectionFailure() {
  FlowBackend b;
  auto d = clickFlow(b);
  d.playback.infinite = true;
  AutomationCoordinator c;
  FlowPlayer p(&b, nullptr);
  p.setDocument(d);
  MacroController runner(nullptr, &p, &c);
  auto connection =
      connect(&runner, &MacroController::stepProgress, &runner, [&] { runner.stop(); });
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  QVERIFY(b.points.isEmpty());
  QCOMPARE(runner.state(), MacroControllerState::Idle);
  disconnect(connection);
  b.success = false;
  QSignalSpy failed(&runner, &MacroController::failed);
  QVERIFY(runner.startPlayback(*FlowTypes::compile(d)));
  QCOMPARE(failed.size(), 1);
  QVERIFY(failed[0][0].toString().contains("步骤 1"));
  QCOMPARE(c.activity(), AutomationActivity::Idle);
}
void FlowTests::captureCancellationAndHotkeyConflict() {
  FlowBackend b;
  FlowEditorPage page;
  page.setDocument(clickFlow(b));
  QTemporaryDir tmp;
  AutomationCoordinator c;
  FlowWorkflowController workflow(&page, &b, nullptr, &c, tmp.path(), [] { return true; });
  workflow.capture();
  c.requestEmergencyStop();
  QTest::qWait(10);
  QCOMPARE(page.document().steps[0].point, QPoint(10, 20));
  workflow.capture(false);
  QCOMPARE(page.document().steps[0].point, QPoint(25, 35));
  auto d = clickFlow(b);
  auto key = FlowTypes::createStep(FlowStepKind::Key);
  key.value = "F11";
  d.steps.append(key);
  page.setDocument(d);
  workflow.setReservedHotkeys([] { return QStringList{"F11"}; });
  emit page.runRequested(false);
  QVERIFY(!workflow.isRunning());
  QVERIFY(b.points.isEmpty());
  QVERIFY(page.findChild<QLabel*>("flowMessage")->text().contains("冲突"));
  auto raw = FlowTypes::createStep(FlowStepKind::Recording);
  raw.recording = rawRecording();
  d.steps = {raw};
  page.setDocument(d);
  workflow.setReservedHotkeys([] { return QStringList{"Ctrl+A"}; });
  emit page.runRequested(false);
  QVERIFY(!workflow.isRunning());
  QVERIFY(page.findChild<QLabel*>("flowMessage")->text().contains("控制热键主键"));
}
QTEST_MAIN(FlowTests)
#include "FlowTests.moc"
