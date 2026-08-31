#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

#include "app/UiStyle.h"
#include "app/widgets/ActionBar.h"
#include "app/widgets/NavItemDelegate.h"
#include "app/widgets/NavigationSidebar.h"
#include "app/widgets/StatusStrip.h"

class ProductShellTests : public QObject {
  Q_OBJECT

 private slots:
  void sidebarHasFourProductPages();
  void persistentRegionsExposeState();
  void navDelegatePaintsFluentSelectionIndicator();
  void navDelegateUsesFluentRowHeight();
};

void ProductShellTests::sidebarHasFourProductPages() {
  QCoreApplication::setApplicationVersion("0.2.0");
  NavigationSidebar sidebar;
  QSignalSpy spy(&sidebar, &NavigationSidebar::pageSelected);

  QCOMPARE(sidebar.pageCount(), 4);
  QCOMPARE(sidebar.productName(), QString("ClickFlow"));
  QCOMPARE(sidebar.versionText(), QString("0.2.0"));

  sidebar.setCurrentPage(ShellPage::Hotkeys);
  QCOMPARE(sidebar.currentPage(), ShellPage::Hotkeys);
  QCOMPARE(spy.count(), 1);

  sidebar.setCurrentPage(ShellPage::MacroRecording);
  QCOMPARE(sidebar.currentPage(), ShellPage::MacroRecording);
}

void ProductShellTests::persistentRegionsExposeState() {
  StatusStrip status;
  status.setPermissionState(true);
  status.setStatus("运行中");
  status.setProgress("剩余 8 次");
  QCOMPARE(status.permissionText(), QString("输入控制权限：可用"));
  QCOMPARE(status.statusText(), QString("运行中"));
  QCOMPARE(status.progressText(), QString("剩余 8 次"));

  ActionBar actions;
  actions.setRunning(true);
  actions.setSummary("100 毫秒 · 跟随鼠标 · 无限");
  QCOMPARE(actions.buttonText(), QString("停止连点"));
  QCOMPARE(actions.summaryText(), QString("100 毫秒 · 跟随鼠标 · 无限"));
}

void ProductShellTests::navDelegatePaintsFluentSelectionIndicator() {
  QListWidget list;
  auto* delegate = new NavItemDelegate(&list);
  list.setItemDelegate(delegate);
  new QListWidgetItem(QStringLiteral("连点设置"), &list);

  QPixmap canvas(160, 36);
  canvas.fill(Qt::transparent);
  QPainter painter(&canvas);
  QStyleOptionViewItem option;
  option.rect = QRect(0, 0, 160, 36);
  option.state = QStyle::State_Enabled | QStyle::State_Selected;
  delegate->paint(&painter, option, list.model()->index(0, 0));
  painter.end();

  // 指示条：左侧 x+4、宽 3、高 16、垂直居中 → 中心采样点 (5, 18)
  const QColor indicator = canvas.toImage().pixelColor(5, 18);
  const QColor accent = fluentLightTokens().accent;
  QCOMPARE(indicator.red(), accent.red());
  QCOMPARE(indicator.green(), accent.green());
  QCOMPARE(indicator.blue(), accent.blue());
}

void ProductShellTests::navDelegateUsesFluentRowHeight() {
  QListWidget list;
  auto* delegate = new NavItemDelegate(&list);
  new QListWidgetItem(QStringLiteral("连点设置"), &list);
  QStyleOptionViewItem option;
  QCOMPARE(delegate->sizeHint(option, list.model()->index(0, 0)).height(), 36);
}

QTEST_MAIN(ProductShellTests)

#include "ProductShellTests.moc"
