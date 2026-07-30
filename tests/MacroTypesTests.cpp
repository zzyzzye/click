#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "core/MacroRepository.h"
#include "core/MacroTypes.h"

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

QTEST_APPLESS_MAIN(MacroTypesTests)

#include "MacroTypesTests.moc"
