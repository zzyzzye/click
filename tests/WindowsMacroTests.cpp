#include <QTest>

#include <memory>

#include "platform/windows/WindowsWindowApi.h"
#include "platform/windows/WindowsWindowService.h"

class FakeWindowsWindowApi final : public WindowsWindowApi {
 public:
  QVector<WindowsNativeWindow> enumerateWindows() const override { return windows; }

  std::optional<WindowsNativeWindow> windowInfo(quintptr id) const override {
    for (const auto& window : windows) {
      if (window.id == id) return window;
    }
    return std::nullopt;
  }

  quintptr rootWindowAt(const QPoint&) const override { return windowAtResult; }
  bool isWindow(quintptr id) const override { return aliveIds.contains(id); }
  quintptr foregroundWindow() const override { return foreground; }
  bool activateWindow(quintptr id) override {
    activated = id;
    return activateResult;
  }
  std::optional<QPoint> screenToClient(quintptr id, const QPoint& point) const override {
    if (!aliveIds.contains(id)) return std::nullopt;
    return point - origins.value(id);
  }
  std::optional<QPoint> clientToScreen(quintptr id, const QPoint& point) const override {
    if (!aliveIds.contains(id)) return std::nullopt;
    return point + origins.value(id);
  }
  QRect virtualDesktopRect() const override { return desktop; }

  QVector<WindowsNativeWindow> windows;
  QSet<quintptr> aliveIds;
  QHash<quintptr, QPoint> origins;
  quintptr windowAtResult = 0;
  quintptr foreground = 0;
  quintptr activated = 0;
  bool activateResult = true;
  QRect desktop{-1920, 0, 3840, 1080};
};

namespace {

WindowsNativeWindow nativeWindow(quintptr id, const QString& title,
                                 const QString& executable,
                                 const QString& className = "AppWindow") {
  WindowsNativeWindow window;
  window.id = id;
  window.visible = true;
  window.topLevel = true;
  window.title = title;
  window.executablePath = executable;
  window.className = className;
  window.clientSize = QSize(800, 600);
  return window;
}

}  // namespace

class WindowsMacroTests : public QObject {
  Q_OBJECT

 private slots:
  void filtersIneligibleWindows();
  void resolvesOnlyUniqueWindowIdentity();
  void picksRootWindowAndConvertsCoordinates();
  void checksRecordedClientSizeTolerance();
};

void WindowsMacroTests::filtersIneligibleWindows() {
  auto api = std::make_unique<FakeWindowsWindowApi>();
  api->windows = {
      nativeWindow(1, "文档 - 记事本", "C:/Windows/notepad.exe"),
      nativeWindow(2, "ClickFlow", "D:/ZW/click/ClickFlow.exe"),
      nativeWindow(3, "隐藏", "C:/hidden.exe"),
      nativeWindow(4, "已遮蔽", "C:/cloaked.exe"),
      nativeWindow(5, "", "C:/untitled.exe"),
  };
  api->windows[2].visible = false;
  api->windows[3].cloaked = true;
  auto* observed = api.get();
  WindowsWindowService service(std::move(api), 2);

  const auto windows = service.availableWindows();

  QCOMPARE(windows.size(), 1);
  QCOMPARE(windows.first().nativeId, quintptr(1));
  QCOMPARE(windows.first().title, QString("文档 - 记事本"));
  QCOMPARE(service.displayName(windows.first()),
           QString("文档 - 记事本 — notepad.exe"));
  QVERIFY(observed->aliveIds.isEmpty());
}

void WindowsMacroTests::resolvesOnlyUniqueWindowIdentity() {
  auto api = std::make_unique<FakeWindowsWindowApi>();
  api->windows = {
      nativeWindow(10, "报告", "C:/Apps/editor.exe", "EditorWindow"),
      nativeWindow(11, "其他", "C:/Apps/editor.exe", "EditorWindow"),
  };
  api->aliveIds = {10, 11};
  WindowsWindowService service(std::move(api));

  WindowTarget exact;
  exact.executablePath = "C:/Apps/editor.exe";
  exact.className = "EditorWindow";
  exact.title = "报告";
  exact.clientSize = QSize(800, 600);
  QString error;
  const auto resolved = service.resolve(exact, &error);
  QVERIFY2(resolved.has_value(), qPrintable(error));
  QCOMPARE(resolved->nativeId, quintptr(10));

  exact.title = "不存在";
  const auto ambiguous = service.resolve(exact, &error);
  QVERIFY(!ambiguous.has_value());
  QVERIFY(error.contains("多个"));
}

void WindowsMacroTests::picksRootWindowAndConvertsCoordinates() {
  auto api = std::make_unique<FakeWindowsWindowApi>();
  api->windows = {nativeWindow(20, "目标", "C:/Apps/target.exe")};
  api->aliveIds = {20};
  api->windowAtResult = 20;
  api->origins.insert(20, QPoint(100, 200));
  api->foreground = 20;
  auto* observed = api.get();
  WindowsWindowService service(std::move(api));

  const auto selected = service.windowAt(QPoint(130, 250));
  QVERIFY(selected.has_value());
  QCOMPARE(selected->nativeId, quintptr(20));
  QCOMPARE(service.screenToClient(*selected, QPoint(130, 250)),
           std::optional<QPoint>(QPoint(30, 50)));
  QCOMPARE(service.clientToScreen(*selected, QPoint(30, 50)),
           std::optional<QPoint>(QPoint(130, 250)));
  QVERIFY(service.isForeground(*selected));
  QVERIFY(service.activate(*selected));
  QCOMPARE(observed->activated, quintptr(20));
  QCOMPARE(service.virtualDesktopRect(), QRect(-1920, 0, 3840, 1080));
}

void WindowsMacroTests::checksRecordedClientSizeTolerance() {
  QVERIFY(WindowService::sizeMatches(QSize(800, 600), QSize(816, 612)));
  QVERIFY(WindowService::sizeMatches(QSize(1000, 1000), QSize(1020, 980)));
  QVERIFY(!WindowService::sizeMatches(QSize(800, 600), QSize(817, 600)));
  QVERIFY(!WindowService::sizeMatches(QSize(1000, 1000), QSize(1021, 1000)));
}

QTEST_APPLESS_MAIN(WindowsMacroTests)

#include "WindowsMacroTests.moc"
