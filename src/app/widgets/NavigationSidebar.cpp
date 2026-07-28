#include "app/widgets/NavigationSidebar.h"

#include <QCoreApplication>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

NavigationSidebar::NavigationSidebar(QWidget* parent) : QFrame(parent) {
  setObjectName("navigationSidebar");
  setFixedWidth(184);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 24, 16, 16);
  layout->setSpacing(8);

  productLabel_ = new QLabel("ClickFlow", this);
  productLabel_->setObjectName("productName");
  versionLabel_ =
      new QLabel(QString("连点器 · %1").arg(QCoreApplication::applicationVersion()), this);
  versionLabel_->setObjectName("productVersion");
  layout->addWidget(productLabel_);
  layout->addWidget(versionLabel_);
  layout->addSpacing(20);

  navigation_ = new QListWidget(this);
  navigation_->setObjectName("sidebarNavigation");
  navigation_->setFrameShape(QFrame::NoFrame);
  navigation_->setSpacing(4);
  const struct {
    QString label;
    ShellPage page;
  } items[] = {
      {"连点设置", ShellPage::ClickSettings},
      {"热键", ShellPage::Hotkeys},
      {"预设与关于", ShellPage::PresetsAbout},
  };
  for (const auto& item : items) {
    auto* row = new QListWidgetItem(item.label, navigation_);
    row->setData(Qt::UserRole, static_cast<int>(item.page));
    row->setSizeHint(QSize(0, 42));
  }
  layout->addWidget(navigation_, 1);

  connect(navigation_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0) {
      emit pageSelected(static_cast<ShellPage>(
          navigation_->item(row)->data(Qt::UserRole).toInt()));
    }
  });
  navigation_->setCurrentRow(0);
}

int NavigationSidebar::pageCount() const {
  return navigation_->count();
}

ShellPage NavigationSidebar::currentPage() const {
  return static_cast<ShellPage>(
      navigation_->currentItem()->data(Qt::UserRole).toInt());
}

void NavigationSidebar::setCurrentPage(ShellPage page) {
  for (int row = 0; row < navigation_->count(); ++row) {
    if (navigation_->item(row)->data(Qt::UserRole).toInt() ==
        static_cast<int>(page)) {
      navigation_->setCurrentRow(row);
      return;
    }
  }
}

QString NavigationSidebar::productName() const {
  return productLabel_->text();
}

QString NavigationSidebar::versionText() const {
  return QCoreApplication::applicationVersion();
}
