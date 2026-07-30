#include <QTest>

#include <memory>
#include <optional>

#include "platform/windows/WindowsClickBackend.h"
#include "platform/windows/WindowsInputApi.h"

class FakeWindowsInputApi final : public WindowsInputApi {
 public:
  std::optional<QPoint> cursorPosition() const override {
    return cursor;
  }

  bool clickAt(const QPoint& point, ClickButton button) override {
    ++clickCalls;
    clickedPoint = point;
    clickedButton = button;
    return clickResult;
  }

  bool keyTap(const QString&) override { return true; }

  std::optional<QPoint> cursor = QPoint(40, 50);
  QPoint clickedPoint;
  ClickButton clickedButton = ClickButton::Left;
  bool clickResult = true;
  int clickCalls = 0;
};

class WindowsClickBackendTests : public QObject {
  Q_OBJECT

 private slots:
  void followCursorUsesCurrentPosition();
  void fixedPointPreservesRightButton();
  void unavailableCursorStopsBeforeClick();
  void inputFailureIsReported();
  void jitterIncludesBothRadiusBounds();
  void zeroJitterDoesNotUseRandomSource();
};

void WindowsClickBackendTests::followCursorUsesCurrentPosition() {
  auto api = std::make_unique<FakeWindowsInputApi>();
  auto* observed = api.get();
  WindowsClickBackend backend(std::move(api), [](int, int) { return 0; });

  ClickProfile profile;
  profile.targetMode = TargetMode::FollowCursor;

  QVERIFY(backend.click(profile));
  QCOMPARE(observed->clickCalls, 1);
  QCOMPARE(observed->clickedPoint, QPoint(40, 50));
  QCOMPARE(observed->clickedButton, ClickButton::Left);
}

void WindowsClickBackendTests::fixedPointPreservesRightButton() {
  auto api = std::make_unique<FakeWindowsInputApi>();
  auto* observed = api.get();
  WindowsClickBackend backend(std::move(api), [](int, int) { return 0; });

  ClickProfile profile;
  profile.targetMode = TargetMode::FixedPoint;
  profile.fixedPoint = QPoint(640, 480);
  profile.button = ClickButton::Right;

  QVERIFY(backend.click(profile));
  QCOMPARE(observed->clickCalls, 1);
  QCOMPARE(observed->clickedPoint, QPoint(640, 480));
  QCOMPARE(observed->clickedButton, ClickButton::Right);
}

void WindowsClickBackendTests::unavailableCursorStopsBeforeClick() {
  auto api = std::make_unique<FakeWindowsInputApi>();
  api->cursor = std::nullopt;
  auto* observed = api.get();
  WindowsClickBackend backend(std::move(api), [](int, int) { return 0; });

  ClickProfile profile;
  profile.targetMode = TargetMode::FollowCursor;

  QVERIFY(!backend.click(profile));
  QCOMPARE(observed->clickCalls, 0);
}

void WindowsClickBackendTests::inputFailureIsReported() {
  auto api = std::make_unique<FakeWindowsInputApi>();
  api->clickResult = false;
  WindowsClickBackend backend(std::move(api), [](int, int) { return 0; });

  ClickProfile profile;
  profile.targetMode = TargetMode::FixedPoint;

  QVERIFY(!backend.click(profile));
}

void WindowsClickBackendTests::jitterIncludesBothRadiusBounds() {
  {
    auto api = std::make_unique<FakeWindowsInputApi>();
    auto* observed = api.get();
    WindowsClickBackend backend(std::move(api),
                                [](int minimum, int) { return minimum; });
    ClickProfile profile;
    profile.targetMode = TargetMode::FixedPoint;
    profile.fixedPoint = QPoint(100, 200);
    profile.jitterRadius = 7;

    QVERIFY(backend.click(profile));
    QCOMPARE(observed->clickedPoint, QPoint(93, 193));
  }

  {
    auto api = std::make_unique<FakeWindowsInputApi>();
    auto* observed = api.get();
    WindowsClickBackend backend(
        std::move(api), [](int, int maximumInclusive) { return maximumInclusive; });
    ClickProfile profile;
    profile.targetMode = TargetMode::FixedPoint;
    profile.fixedPoint = QPoint(100, 200);
    profile.jitterRadius = 7;

    QVERIFY(backend.click(profile));
    QCOMPARE(observed->clickedPoint, QPoint(107, 207));
  }
}

void WindowsClickBackendTests::zeroJitterDoesNotUseRandomSource() {
  auto api = std::make_unique<FakeWindowsInputApi>();
  int randomCalls = 0;
  WindowsClickBackend backend(
      std::move(api), [&randomCalls](int, int) {
        ++randomCalls;
        return 0;
      });

  ClickProfile profile;
  profile.targetMode = TargetMode::FixedPoint;
  profile.jitterRadius = 0;

  QVERIFY(backend.click(profile));
  QCOMPARE(randomCalls, 0);
}

QTEST_APPLESS_MAIN(WindowsClickBackendTests)

#include "WindowsClickBackendTests.moc"
