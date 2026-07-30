#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "core/MacroRepository.h"
#include "core/MacroTypes.h"
#include "core/AutomationCoordinator.h"
#include "core/MacroCompressor.h"

namespace {

MacroSequence sampleSequence() {
  MacroSequence sequence;
  sequence.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  sequence.name = QStringLiteral("记事本流程");
  sequence.createdAt = QDateTime::fromString("2026-07-30T10:00:00.000Z", Qt::ISODateWithMs);
  sequence.modifiedAt = QDateTime::fromString("2026-07-30T10:01:00.000Z", Qt::ISODateWithMs);
  sequence.targetMode = MacroTargetMode::Window;
  sequence.target.executablePath = QStringLiteral("C:/Windows/System32/notepad.exe");
  sequence.target.className = QStringLiteral("Notepad");
  sequence.target.title = QStringLiteral("无标题 - 记事本");
  sequence.target.clientSize = QSize(800, 600);
  sequence.durationUs = 240000;
  sequence.playback.speed = 1.5;
  sequence.playback.infinite = false;
  sequence.playback.repeatCount = 3;
  sequence.playback.loopDelayMs = 250;

  MacroEvent keyDown;
  keyDown.type = MacroEventType::KeyDown;
  keyDown.offsetUs = 1200;
  keyDown.virtualKey = 'A';
  keyDown.scanCode = 0x1e;
  sequence.events.append(keyDown);

  MacroEvent move;
  move.type = MacroEventType::MouseMove;
  move.offsetUs = 120000;
  move.point = QPoint(125, 240);
  sequence.events.append(move);

  MacroEvent wheel;
  wheel.type = MacroEventType::HorizontalWheel;
  wheel.offsetUs = 240000;
  wheel.wheelDelta = -120;
  sequence.events.append(wheel);
  return sequence;
}

}  // namespace

class MacroTypesTests : public QObject {
  Q_OBJECT

 private slots:
  void jsonRoundTripPreservesAllFields();
  void repositorySavesRenamesAndDeletesByUuid();
  void malformedFileDoesNotHideValidMacros();
  void coordinatorRejectsCompetingActivity();
  void compressorPreservesTransitionsAndSimplifiesMoves();
  void removesTerminalReservedHotkeyChord();
};

void MacroTypesTests::jsonRoundTripPreservesAllFields() {
  const MacroSequence expected = sampleSequence();
  QString error;

  const auto actual = MacroTypes::fromJson(MacroTypes::toJson(expected), &error);

  QVERIFY2(actual.has_value(), qPrintable(error));
  QCOMPARE(actual->id, expected.id);
  QCOMPARE(actual->name, expected.name);
  QCOMPARE(actual->createdAt, expected.createdAt);
  QCOMPARE(actual->modifiedAt, expected.modifiedAt);
  QCOMPARE(actual->targetMode, expected.targetMode);
  QCOMPARE(actual->target.nativeId, quintptr(0));
  QCOMPARE(actual->target.executablePath, expected.target.executablePath);
  QCOMPARE(actual->target.className, expected.target.className);
  QCOMPARE(actual->target.title, expected.target.title);
  QCOMPARE(actual->target.clientSize, expected.target.clientSize);
  QCOMPARE(actual->durationUs, expected.durationUs);
  QCOMPARE(actual->playback.speed, expected.playback.speed);
  QCOMPARE(actual->playback.infinite, expected.playback.infinite);
  QCOMPARE(actual->playback.repeatCount, expected.playback.repeatCount);
  QCOMPARE(actual->playback.loopDelayMs, expected.playback.loopDelayMs);
  QCOMPARE(actual->events.size(), expected.events.size());
  for (qsizetype index = 0; index < expected.events.size(); ++index) {
    const auto& lhs = actual->events[index];
    const auto& rhs = expected.events[index];
    QCOMPARE(lhs.type, rhs.type);
    QCOMPARE(lhs.offsetUs, rhs.offsetUs);
    QCOMPARE(lhs.virtualKey, rhs.virtualKey);
    QCOMPARE(lhs.scanCode, rhs.scanCode);
    QCOMPARE(lhs.nativeFlags, rhs.nativeFlags);
    QCOMPARE(lhs.point, rhs.point);
    QCOMPARE(lhs.button, rhs.button);
    QCOMPARE(lhs.wheelDelta, rhs.wheelDelta);
  }
}

void MacroTypesTests::repositorySavesRenamesAndDeletesByUuid() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  MacroRepository repository(directory.path());
  MacroSequence sequence = sampleSequence();
  QString error;

  QVERIFY2(repository.save(sequence, &error), qPrintable(error));
  QVERIFY(QFile::exists(directory.filePath(sequence.id + ".json")));
  auto loaded = repository.loadAll();
  QCOMPARE(loaded.size(), 1);
  QCOMPARE(loaded.first().name, sequence.name);

  QVERIFY2(repository.rename(sequence.id, QStringLiteral("新的名称"), &error),
           qPrintable(error));
  loaded = repository.loadAll();
  QCOMPARE(loaded.size(), 1);
  QCOMPARE(loaded.first().name, QStringLiteral("新的名称"));
  QCOMPARE(loaded.first().id, sequence.id);

  QVERIFY2(repository.remove(sequence.id, &error), qPrintable(error));
  QVERIFY(repository.loadAll().isEmpty());
  QVERIFY(!QFile::exists(directory.filePath(sequence.id + ".json")));
}

void MacroTypesTests::malformedFileDoesNotHideValidMacros() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  MacroRepository repository(directory.path());
  const MacroSequence sequence = sampleSequence();
  QString error;
  QVERIFY2(repository.save(sequence, &error), qPrintable(error));

  QFile malformed(directory.filePath("broken.json"));
  QVERIFY(malformed.open(QIODevice::WriteOnly));
  QCOMPARE(malformed.write("{not-json"), qint64(9));
  malformed.close();

  QStringList warnings;
  const auto loaded = repository.loadAll(&warnings);
  QCOMPARE(loaded.size(), 1);
  QCOMPARE(loaded.first().id, sequence.id);
  QCOMPARE(warnings.size(), 1);
  QVERIFY(warnings.first().contains("broken.json"));
}

void MacroTypesTests::coordinatorRejectsCompetingActivity() {
  AutomationCoordinator coordinator;
  QString error;

  QVERIFY(coordinator.tryAcquire(AutomationActivity::Recording, &error));
  QCOMPARE(coordinator.activity(), AutomationActivity::Recording);
  QVERIFY(!coordinator.tryAcquire(AutomationActivity::Clicking, &error));
  QVERIFY(error.contains(QStringLiteral("录制")));

  coordinator.release(AutomationActivity::Clicking);
  QCOMPARE(coordinator.activity(), AutomationActivity::Recording);
  coordinator.release(AutomationActivity::Recording);
  QCOMPARE(coordinator.activity(), AutomationActivity::Idle);

  QSignalSpy emergencySpy(&coordinator,
                          &AutomationCoordinator::emergencyStopRequested);
  coordinator.requestEmergencyStop();
  QCOMPARE(emergencySpy.count(), 1);
}

void MacroTypesTests::compressorPreservesTransitionsAndSimplifiesMoves() {
  QVector<MacroEvent> events;
  for (int index = 0; index < 6; ++index) {
    MacroEvent move;
    move.type = MacroEventType::MouseMove;
    move.offsetUs = index * 4000;
    move.point = QPoint(index * 2, index * 2);
    events.append(move);
  }
  MacroEvent down;
  down.type = MacroEventType::MouseButtonDown;
  down.offsetUs = 21000;
  down.point = QPoint(10, 10);
  down.button = MacroMouseButton::Left;
  events.append(down);
  MacroEvent drag = down;
  drag.type = MacroEventType::MouseMove;
  drag.offsetUs = 25000;
  drag.point = QPoint(20, 20);
  events.append(drag);
  MacroEvent up = down;
  up.type = MacroEventType::MouseButtonUp;
  up.offsetUs = 30000;
  up.point = QPoint(20, 20);
  events.append(up);

  const auto compressed = MacroCompressor::compress(events);

  QVERIFY(compressed.size() < events.size());
  QCOMPARE(compressed.first().point, QPoint(0, 0));
  QCOMPARE(compressed.last().type, MacroEventType::MouseButtonUp);
  QCOMPARE(compressed.last().button, MacroMouseButton::Left);
  QVERIFY(std::any_of(compressed.cbegin(), compressed.cend(), [](const MacroEvent& event) {
    return event.type == MacroEventType::MouseButtonDown;
  }));
  QVERIFY(std::any_of(compressed.cbegin(), compressed.cend(), [](const MacroEvent& event) {
    return event.type == MacroEventType::MouseMove && event.point == QPoint(20, 20);
  }));
}

void MacroTypesTests::removesTerminalReservedHotkeyChord() {
  QVector<MacroEvent> events;
  MacroEvent typed;
  typed.type = MacroEventType::KeyDown;
  typed.offsetUs = 1000;
  typed.virtualKey = 'A';
  events.append(typed);
  typed.type = MacroEventType::KeyUp;
  typed.offsetUs = 2000;
  events.append(typed);

  MacroEvent control;
  control.type = MacroEventType::KeyDown;
  control.offsetUs = 3000;
  control.virtualKey = 0x11;
  events.append(control);
  MacroEvent f9 = control;
  f9.virtualKey = 0x78;
  f9.offsetUs = 3100;
  events.append(f9);
  f9.type = MacroEventType::KeyUp;
  f9.offsetUs = 3200;
  events.append(f9);
  control.type = MacroEventType::KeyUp;
  control.offsetUs = 3300;
  events.append(control);

  const auto filtered =
      MacroCompressor::removeReservedTail(events, {QStringLiteral("Ctrl+F9")});

  QCOMPARE(filtered.size(), 2);
  QCOMPARE(filtered.first().virtualKey, quint32('A'));
  QCOMPARE(filtered.last().type, MacroEventType::KeyUp);
}

QTEST_APPLESS_MAIN(MacroTypesTests)

#include "MacroTypesTests.moc"
