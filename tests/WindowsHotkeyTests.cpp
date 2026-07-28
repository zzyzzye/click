#include <QTest>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "platform/windows/WindowsHotkeyMapping.h"

class WindowsHotkeyTests : public QObject {
  Q_OBJECT

 private slots:
  void mapsSupportedSequences_data();
  void mapsSupportedSequences();
  void rejectsUnsupportedSequences_data();
  void rejectsUnsupportedSequences();
};

void WindowsHotkeyTests::mapsSupportedSequences_data() {
  QTest::addColumn<QString>("sequence");
  QTest::addColumn<quint32>("modifiers");
  QTest::addColumn<quint32>("virtualKey");

  constexpr quint32 repeat = MOD_NOREPEAT;
  QTest::newRow("f1") << QString("F1") << repeat << quint32(VK_F1);
  QTest::newRow("f12") << QString("F12") << repeat << quint32(VK_F12);
  QTest::newRow("a") << QString("A") << repeat << quint32('A');
  QTest::newRow("z") << QString("Z") << repeat << quint32('Z');
  QTest::newRow("zero") << QString("0") << repeat << quint32('0');
  QTest::newRow("nine") << QString("9") << repeat << quint32('9');
  QTest::newRow("space") << QString("Space") << repeat << quint32(VK_SPACE);
  QTest::newRow("return") << QString("Return") << repeat << quint32(VK_RETURN);
  QTest::newRow("enter") << QString("Enter") << repeat << quint32(VK_RETURN);
  QTest::newRow("escape") << QString("Esc") << repeat << quint32(VK_ESCAPE);
  QTest::newRow("ctrl-shift-f6")
      << QString("Ctrl+Shift+F6")
      << quint32(MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT) << quint32(VK_F6);
  QTest::newRow("alt-a")
      << QString("Alt+A") << quint32(MOD_ALT | MOD_NOREPEAT) << quint32('A');
  QTest::newRow("meta-z")
      << QString("Meta+Z") << quint32(MOD_WIN | MOD_NOREPEAT) << quint32('Z');
}

void WindowsHotkeyTests::mapsSupportedSequences() {
  QFETCH(QString, sequence);
  QFETCH(quint32, modifiers);
  QFETCH(quint32, virtualKey);

  const auto binding = mapWindowsHotkey(sequence);

  QVERIFY(binding.has_value());
  QCOMPARE(binding->modifiers, modifiers);
  QCOMPARE(binding->virtualKey, virtualKey);
}

void WindowsHotkeyTests::rejectsUnsupportedSequences_data() {
  QTest::addColumn<QString>("sequence");

  QTest::newRow("empty") << QString();
  QTest::newRow("multi-stroke") << QString("Ctrl+K, Ctrl+C");
  QTest::newRow("unsupported-tab") << QString("Tab");
  QTest::newRow("modifier-only") << QString("Ctrl");
}

void WindowsHotkeyTests::rejectsUnsupportedSequences() {
  QFETCH(QString, sequence);
  QVERIFY(!mapWindowsHotkey(sequence).has_value());
}

QTEST_APPLESS_MAIN(WindowsHotkeyTests)

#include "WindowsHotkeyTests.moc"
