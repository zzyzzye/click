#include <QSignalSpy>
#include <QTest>

#include "app/widgets/ActionBar.h"
#include "app/widgets/NavigationSidebar.h"
#include "app/widgets/StatusStrip.h"

class ProductShellTests : public QObject {
  Q_OBJECT

 private slots:
  void sidebarHasThreeProductPages();
  void persistentRegionsExposeState();
};

void ProductShellTests::sidebarHasThreeProductPages() {
  QCoreApplication::setApplicationVersion("0.2.0");
  NavigationSidebar sidebar;
  QSignalSpy spy(&sidebar, &NavigationSidebar::pageSelected);

  QCOMPARE(sidebar.pageCount(), 3);
  QCOMPARE(sidebar.productName(), QString("ClickFlow"));
  QCOMPARE(sidebar.versionText(), QString("0.2.0"));

  sidebar.setCurrentPage(ShellPage::Hotkeys);
  QCOMPARE(sidebar.currentPage(), ShellPage::Hotkeys);
  QCOMPARE(spy.count(), 1);
}

void ProductShellTests::persistentRegionsExposeState() {
  StatusStrip status;
  status.setPermissionState(true);
  status.setStatus("运行中");
  status.setProgress("剩余 8 次");
  QCOMPARE(status.permissionText(), QString("输入控制可用"));
  QCOMPARE(status.statusText(), QString("运行中"));
  QCOMPARE(status.progressText(), QString("剩余 8 次"));

  ActionBar actions;
  actions.setRunning(true);
  actions.setSummary("100 毫秒 · 跟随鼠标 · 无限");
  QCOMPARE(actions.buttonText(), QString("停止连点"));
  QCOMPARE(actions.summaryText(), QString("100 毫秒 · 跟随鼠标 · 无限"));
}

QTEST_MAIN(ProductShellTests)

#include "ProductShellTests.moc"
