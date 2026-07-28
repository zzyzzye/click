#include "app/pages/PresetsAboutPage.h"
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

PresetsAboutPage::PresetsAboutPage(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(16);
  auto* presets = new QFrame(this); presets->setObjectName("settingsCard");
  auto* p = new QVBoxLayout(presets); auto* title = new QLabel("配置预设", presets); title->setObjectName("cardTitle"); p->addWidget(title);
  list_ = new QListWidget(presets); p->addWidget(list_, 1);
  auto* tools = new QHBoxLayout();
  new_ = new QPushButton("新建"); save_ = new QPushButton("保存"); rename_ = new QPushButton("重命名");
  delete_ = new QPushButton("删除"); load_ = new QPushButton("加载");
  for (auto* b : {new_, save_, rename_, delete_, load_}) tools->addWidget(b);
  tools->addStretch(); p->addLayout(tools); root->addWidget(presets, 1);
  auto* about = new QFrame(this); about->setObjectName("settingsCard"); auto* a = new QVBoxLayout(about);
  product_ = new QLabel("ClickFlow", about); product_->setObjectName("cardTitle");
  version_ = new QLabel(QCoreApplication::applicationVersion(), about);
#if defined(Q_OS_WIN)
  platform_ = new QLabel("Windows", about);
#elif defined(Q_OS_MACOS)
  platform_ = new QLabel("macOS", about);
#else
  platform_ = new QLabel("Unknown", about);
#endif
  qt_ = new QLabel(qVersion(), about);
  a->addWidget(product_); a->addWidget(version_); a->addWidget(platform_); a->addWidget(qt_);
  root->addWidget(about);
  connect(list_, &QListWidget::itemSelectionChanged, this, [this]{ updateActions(); emit selectionChanged(); });
  connect(new_, &QPushButton::clicked, this, &PresetsAboutPage::newRequested);
  connect(save_, &QPushButton::clicked, this, &PresetsAboutPage::saveRequested);
  connect(rename_, &QPushButton::clicked, this, &PresetsAboutPage::renameRequested);
  connect(delete_, &QPushButton::clicked, this, &PresetsAboutPage::deleteRequested);
  connect(load_, &QPushButton::clicked, this, &PresetsAboutPage::loadRequested);
  updateActions();
}
void PresetsAboutPage::setPresetNames(const QStringList& names, const QString& selected) {
  list_->clear(); list_->addItems(names);
  const auto matches = list_->findItems(selected, Qt::MatchExactly);
  if (!matches.isEmpty()) list_->setCurrentItem(matches.first());
  updateActions();
}
QString PresetsAboutPage::selectedPresetName() const { return list_->currentItem() ? list_->currentItem()->text() : QString(); }
QString PresetsAboutPage::productName() const { return product_->text(); }
QString PresetsAboutPage::versionText() const { return version_->text(); }
QString PresetsAboutPage::platformText() const { return platform_->text(); }
QString PresetsAboutPage::qtVersionText() const { return qt_->text(); }
void PresetsAboutPage::setMutationEnabled(bool enabled) { new_->setEnabled(enabled); save_->setEnabled(enabled); rename_->setEnabled(enabled && list_->currentItem()); delete_->setEnabled(enabled && list_->currentItem()); load_->setEnabled(enabled && list_->currentItem()); }
void PresetsAboutPage::updateActions() { const bool selected = list_->currentItem(); rename_->setEnabled(selected); delete_->setEnabled(selected); load_->setEnabled(selected); }
