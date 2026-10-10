#include "app/widgets/NavigationSidebar.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QPixmap>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

#include "app/widgets/NavItemDelegate.h"

NavigationSidebar::NavigationSidebar(QWidget* parent) : QFrame(parent) {
  setObjectName("navigationSidebar");
  setFixedWidth(200);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 28, 16, 20);
  layout->setSpacing(8);

  productLabel_ = new QLabel("ClickFlow", this);
  productLabel_->setObjectName("productName");
  versionLabel_ =
      new QLabel(QString("连点器 · %1").arg(QCoreApplication::applicationVersion()), this);
  versionLabel_->setObjectName("productVersion");
  auto* brand = new QHBoxLayout;
  brand->setSpacing(8);
  auto* logo = new QLabel(this);
  logo->setObjectName("brandLogo");
  logo->setAccessibleName("ClickFlow 标志");
  logo->setFixedSize(44, 44);
  // 使用资源原图，按设备像素比缩放，避免 Retina 屏模糊。
  QPixmap pixmap(":/clickflow/icons/ClickFlow.png");
  pixmap = pixmap.scaled(QSize(44, 44) * devicePixelRatioF(),
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
  pixmap.setDevicePixelRatio(devicePixelRatioF());
  logo->setPixmap(pixmap);
  brand->addWidget(logo);
  auto* brandText = new QVBoxLayout;
  brandText->setSpacing(3);
  brandText->addWidget(productLabel_);
  brandText->addWidget(versionLabel_);
  brand->addLayout(brandText, 1);
  layout->addLayout(brand);
  layout->addSpacing(30);
  auto* section = new QLabel("工作空间", this);
  section->setObjectName("sidebarSection");
  layout->addWidget(section);

  navigation_ = new QListWidget(this);
  navigation_->setObjectName("sidebarNavigation");
  navigation_->setFrameShape(QFrame::NoFrame);
  navigation_->setSpacing(4);
  navigation_->setItemDelegate(new NavItemDelegate(navigation_));
  const struct {
    QString label;
    ShellPage page;
  } items[] = {
      {"连点设置", ShellPage::ClickSettings},
      {"键鼠录制", ShellPage::MacroRecording},
      {"热键", ShellPage::Hotkeys},
      {"预设与关于", ShellPage::PresetsAbout},
  };
  for (const auto& item : items) {
    auto* row = new QListWidgetItem(item.label, navigation_);
    row->setData(Qt::UserRole, static_cast<int>(item.page));
    row->setSizeHint(QSize(0, 44));
  }
  layout->addWidget(navigation_, 1);
  auto* reducedMotion = new QCheckBox("减少动态效果", this);
  reducedMotion->setObjectName("reduceMotionCheck");
  reducedMotion->setToolTip("关闭页面淡入和滚轮缓动，操作立即响应。");
  layout->addWidget(reducedMotion);
  connect(reducedMotion, &QCheckBox::toggled, this,
          &NavigationSidebar::reduceMotionChanged);
  auto* footer = new QLabel("桌面自动化 · 随时掌控", this);
  footer->setObjectName("sidebarFooter");
  layout->addWidget(footer);

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
