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
    if (!applyingProfile_) repository_->saveLastUsedProfile(profile);
    actionBar_->setSummary(clickPage_->summary());
    emit statusChanged("配置已自动保存");
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
  repository_->saveLastUsedProfile(profile);
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
  repository_->saveProfile(profile);
  refreshPresetList(profile.name);
}

void ProfileController::createPreset() {
  bool accepted = false;
  const QString name =
      QInputDialog::getText(dialogParent_, "新建配置", "配置名称",
                            QLineEdit::Normal, {}, &accepted)
          .trimmed();
  if (!accepted || name.isEmpty()) return;
  if (repository_->hasProfile(name)) {
    QMessageBox::warning(dialogParent_, "无法新建", "已经存在同名配置。");
    return;
  }

  ClickProfile profile = currentProfile();
  profile.name = name;
  repository_->saveProfile(profile);
  applyProfile(profile);
  refreshPresetList(name);
}

void ProfileController::renamePreset() {
  const QString oldName = presetsPage_->selectedPresetName();
  if (oldName.isEmpty()) return;

  bool accepted = false;
  const QString name =
      QInputDialog::getText(dialogParent_, "重命名配置", "新名称",
                            QLineEdit::Normal, oldName, &accepted)
          .trimmed();
  if (accepted && repository_->renameProfile(oldName, name)) {
    currentProfileName_ = name;
    refreshPresetList(name);
  }
}

void ProfileController::deletePreset() {
  const QString name = presetsPage_->selectedPresetName();
  if (name.isEmpty()) return;
  if (QMessageBox::question(dialogParent_, "删除配置",
                            QString("确定删除“%1”吗？").arg(name)) !=
      QMessageBox::Yes) {
    return;
  }

  repository_->deleteProfile(name);
  currentProfileName_ = "Default";
  refreshPresetList();
}

void ProfileController::loadPreset() {
  const auto profile =
      repository_->loadProfile(presetsPage_->selectedPresetName());
  if (!profile.has_value()) return;
  applyProfile(*profile);
  repository_->saveLastUsedProfile(*profile);
}
