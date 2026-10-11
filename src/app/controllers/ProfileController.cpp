#include "app/controllers/ProfileController.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QWidget>

#include "app/controllers/WindowChromeController.h"
#include "app/pages/ClickSettingsPage.h"
#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/PresetsAboutPage.h"
#include "app/widgets/ActionBar.h"
#include "core/SettingsRepository.h"

ProfileController::ProfileController(
    std::unique_ptr<SettingsRepository> repository,
    ClickSettingsPage* clickPage, HotkeySettingsPage* hotkeyPage,
    PresetsAboutPage* presetsPage, ActionBar* actionBar,
    WindowChromeController* windowChrome, QWidget* dialogParent,
    QObject* parent)
    : QObject(parent),
      repository_(std::move(repository)),
      clickPage_(clickPage),
      hotkeyPage_(hotkeyPage),
      presetsPage_(presetsPage),
      actionBar_(actionBar),
      windowChrome_(windowChrome),
      dialogParent_(dialogParent) {
  Q_ASSERT(repository_);
  Q_ASSERT(clickPage_);
  Q_ASSERT(hotkeyPage_);
  Q_ASSERT(presetsPage_);
  Q_ASSERT(actionBar_);
  Q_ASSERT(windowChrome_);
  Q_ASSERT(dialogParent_);

  connect(clickPage_, &ClickSettingsPage::alwaysOnTopChanged, this,
          [this](bool enabled) { windowChrome_->setAlwaysOnTop(enabled); });
  connect(clickPage_, &ClickSettingsPage::settingsChanged, this, [this] {
    const ClickProfile profile = currentProfile();
    const bool saved = applyingProfile_ || repository_->saveLastUsedProfile(profile);
    actionBar_->setSummary(clickPage_->summary());
    if (!applyingProfile_)
      emit statusChanged(saved ? "连点设置已自动保存"
                               : "连点设置保存失败，请检查磁盘空间和写入权限。");
  });

  connect(presetsPage_, &PresetsAboutPage::saveRequested, this,
          &ProfileController::savePreset);
  connect(presetsPage_, &PresetsAboutPage::newRequested, this,
          &ProfileController::createPreset);
  connect(presetsPage_, &PresetsAboutPage::renameRequested, this,
          &ProfileController::renamePreset);
  connect(presetsPage_, &PresetsAboutPage::deleteRequested, this,
          &ProfileController::deletePreset);
  connect(presetsPage_, &PresetsAboutPage::loadRequested, this,
          &ProfileController::loadPreset);
}

ProfileController::~ProfileController() = default;

void ProfileController::initialize() {
  ClickProfile profile;
  if (const auto saved = repository_->loadLastUsedProfile(); saved.has_value()) {
    profile = *saved;
  }
  applyProfile(profile);
  refreshPresetList(profile.name);
}

ClickProfile ProfileController::currentProfile() const {
  ClickProfile profile;
  profile.name = currentProfileName_;
  clickPage_->applyToProfile(profile);
  hotkeyPage_->applyToProfile(profile);
  return profile;
}

void ProfileController::saveLastUsed(const ClickProfile& profile) {
  if (!repository_->saveLastUsedProfile(profile))
    emit statusChanged("连点设置保存失败，请检查磁盘空间和写入权限。");
}

bool ProfileController::macroSafetyAcknowledged() const {
  return repository_->macroSafetyAcknowledged();
}

void ProfileController::acknowledgeMacroSafety() {
  repository_->setMacroSafetyAcknowledged(true);
}

void ProfileController::applyProfile(const ClickProfile& profile) {
  applyingProfile_ = true;
  currentProfileName_ = profile.name;
  presetsPage_->setCurrentProfileName(currentProfileName_);
  clickPage_->setProfile(profile);
  hotkeyPage_->setProfile(profile);
  actionBar_->setSummary(clickPage_->summary());
  windowChrome_->setAlwaysOnTop(profile.alwaysOnTop);
  applyingProfile_ = false;
  emit profileApplied(profile);
}

void ProfileController::refreshPresetList(const QString& selected) {
  QStringList names = repository_->profileNames();
  names.sort(Qt::CaseInsensitive);
  presetsPage_->setPresetNames(names, selected);
}

void ProfileController::savePreset() {
  const ClickProfile profile = currentProfile();
  if (!repository_->saveProfile(profile)) {
    emit statusChanged("预设保存失败，请检查名称、磁盘空间和写入权限；当前设置仍保留。");
    return;
  }
  refreshPresetList(profile.name);
  emit statusChanged(QString("已保存预设“%1”").arg(profile.name));
}

void ProfileController::createPreset() {
  bool accepted = false;
  const QString name =
      QInputDialog::getText(dialogParent_, "新建配置", "配置名称",
                            QLineEdit::Normal, {}, &accepted)
          .trimmed();
  if (!accepted || name.isEmpty()) return;
  if (!SettingsRepository::validProfileName(name)) {
    QMessageBox::warning(dialogParent_, "无法新建", "配置名称不能包含 / 或 \\，请更换名称。");
    return;
  }
  if (repository_->hasProfile(name)) {
    QMessageBox::warning(dialogParent_, "无法新建", "已经存在同名配置。");
    return;
  }

  ClickProfile profile = currentProfile();
  profile.name = name;
  if (!repository_->saveProfile(profile)) {
    emit statusChanged("新建预设失败，请检查磁盘空间和写入权限；当前设置仍保留。");
    return;
  }
  applyProfile(profile);
  saveLastUsed(profile);
  refreshPresetList(name);
  emit statusChanged(QString("已新建并使用预设“%1”").arg(name));
}

void ProfileController::renamePreset() {
  const QString oldName = presetsPage_->selectedPresetName();
  if (oldName.isEmpty()) return;

  bool accepted = false;
  const QString name =
      QInputDialog::getText(dialogParent_, "重命名配置", "新名称",
                            QLineEdit::Normal, oldName, &accepted)
          .trimmed();
  if (!accepted || name == oldName)
    return;
  if (!SettingsRepository::validProfileName(name) || repository_->hasProfile(name)) {
    QMessageBox::warning(dialogParent_, "无法重命名",
                         "名称不能为空、不能包含 / 或 \\，也不能与已有预设同名。");
    return;
  }
  if (!repository_->renameProfile(oldName, name)) {
    emit statusChanged("重命名失败，请检查写入权限；请重新查看预设列表。");
    refreshPresetList(oldName);
    return;
  }
  // 列表选中项不一定是当前使用的预设，不能将另一套设置绑定到新名称。
  if (currentProfileName_ == oldName) {
    currentProfileName_ = name;
    presetsPage_->setCurrentProfileName(currentProfileName_);
    saveLastUsed(currentProfile());
  }
  refreshPresetList(name);
  emit statusChanged(QString("已将预设“%1”重命名为“%2”").arg(oldName, name));
}

void ProfileController::deletePreset() {
  const QString name = presetsPage_->selectedPresetName();
  if (name.isEmpty()) return;
  if (QMessageBox::question(dialogParent_, "删除配置",
                            QString("确定删除“%1”吗？").arg(name)) !=
      QMessageBox::Yes) {
    return;
  }

  if (!repository_->deleteProfile(name)) {
    emit statusChanged("删除预设失败，请检查写入权限后重试。");
    return;
  }
  if (currentProfileName_ == name) {
    currentProfileName_ = "Default";
    presetsPage_->setCurrentProfileName(currentProfileName_);
    saveLastUsed(currentProfile());
  }
  refreshPresetList();
  emit statusChanged(QString("已删除预设“%1”，当前连点参数保留。").arg(name));
}

void ProfileController::loadPreset() {
  const auto profile =
      repository_->loadProfile(presetsPage_->selectedPresetName());
  if (!profile.has_value()) return;
  applyProfile(*profile);
  saveLastUsed(*profile);
}
