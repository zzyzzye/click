#include <QTest>
#include <QSignalSpy>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "core/ClickTypes.h"
#include "platform/windows/WindowsHotkeyApi.h"
#include "platform/windows/WindowsHotkeyMapping.h"
#include "platform/windows/WindowsHotkeyService.h"

struct RegisteredHotkey {
  int id;
  quint32 modifiers;
  quint32 virtualKey;
};

class FakeWindowsHotkeyApi final : public WindowsHotkeyApi {
 public:
  bool registerHotkey(int id, quint32 modifiers, quint32 virtualKey) override {
    attempts.push_back({id, modifiers, virtualKey});
    if (id == failingId) {
      return false;
    }
    activeIds.push_back(id);
    return true;
  }

  void unregisterHotkey(int id) override {
    unregisteredIds.push_back(id);
    activeIds.erase(std::remove(activeIds.begin(), activeIds.end(), id),
                    activeIds.end());
  }

  int failingId = -1;
  std::vector<RegisteredHotkey> attempts;
  std::vector<int> activeIds;
  std::vector<int> unregisteredIds;
};

class WindowsHotkeyTests : public QObject {
  Q_OBJECT

 private slots:
  void mapsSupportedSequences_data();
  void mapsSupportedSequences();
  void rejectsUnsupportedSequences_data();
  void rejectsUnsupportedSequences();
  void registersAllProfileBindings();
  void rollsBackPartialRegistration();
  void dispatchesRegisteredActions();
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

void WindowsHotkeyTests::registersAllProfileBindings() {
  auto api = std::make_unique<FakeWindowsHotkeyApi>();
  auto* observed = api.get();
  WindowsHotkeyService service(std::move(api));

  ClickProfile profile;
  profile.hotkeys.startStop = "Ctrl+F6";
  profile.hotkeys.capturePoint = "Alt+F7";
  profile.hotkeys.emergencyStop = "Shift+F8";

  QVERIFY(service.registerHotkeys(profile));
  QCOMPARE(observed->attempts.size(), size_t(3));
  QCOMPARE(observed->activeIds, std::vector<int>({1, 2, 3}));
  QCOMPARE(observed->attempts[0].virtualKey, quint32(VK_F6));
  QCOMPARE(observed->attempts[1].virtualKey, quint32(VK_F7));
  QCOMPARE(observed->attempts[2].virtualKey, quint32(VK_F8));
}

void WindowsHotkeyTests::rollsBackPartialRegistration() {
  auto api = std::make_unique<FakeWindowsHotkeyApi>();
  api->failingId = 2;
  auto* observed = api.get();
  WindowsHotkeyService service(std::move(api));
  QSignalSpy failureSpy(&service, &HotkeyService::registrationFailed);

  QVERIFY(!service.registerHotkeys(ClickProfile{}));
  QCOMPARE(observed->attempts.size(), size_t(2));
  QVERIFY(observed->activeIds.empty());
  QCOMPARE(observed->unregisteredIds, std::vector<int>({1}));
  QCOMPARE(failureSpy.count(), 1);
  QVERIFY(failureSpy.first().first().toString().contains("F7"));
}

void WindowsHotkeyTests::dispatchesRegisteredActions() {
  auto api = std::make_unique<FakeWindowsHotkeyApi>();
  WindowsHotkeyService service(std::move(api));
  QVERIFY(service.registerHotkeys(ClickProfile{}));

  QSignalSpy startStopSpy(&service, &HotkeyService::startStopPressed);
  QSignalSpy captureSpy(&service, &HotkeyService::capturePointPressed);
  QSignalSpy emergencySpy(&service, &HotkeyService::emergencyStopPressed);

  qintptr result = 0;
  MSG message{};
  message.message = WM_HOTKEY;

  message.wParam = 1;
  service.nativeEventFilter("windows_generic_MSG", &message, &result);
  message.wParam = 2;
  service.nativeEventFilter("windows_generic_MSG", &message, &result);
  message.wParam = 3;
  service.nativeEventFilter("windows_generic_MSG", &message, &result);
  message.wParam = 99;
  service.nativeEventFilter("windows_generic_MSG", &message, &result);

  QCOMPARE(startStopSpy.count(), 1);
  QCOMPARE(captureSpy.count(), 1);
  QCOMPARE(emergencySpy.count(), 1);
}

QTEST_APPLESS_MAIN(WindowsHotkeyTests)

#include "WindowsHotkeyTests.moc"
