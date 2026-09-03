#include "app/controllers/HotkeyController.h"

#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/MacroRecordingPage.h"
#include "app/widgets/ActionBar.h"
#include "core/HotkeyService.h"

HotkeyController::HotkeyController(std::unique_ptr<HotkeyService> service,
                                   HotkeySettingsPage* hotkeyPage,
                                   MacroRecordingPage* macroPage,
                                   ActionBar* actionBar,
                                   ProfileProvider profileProvider,
                                   QObject* parent)
    : QObject(parent),
      service_(std::move(service)),
      hotkeyPage_(hotkeyPage),
      macroPage_(macroPage),
      actionBar_(actionBar),
      profileProvider_(std::move(profileProvider)) {
  Q_ASSERT(service_);
  Q_ASSERT(hotkeyPage_);
  Q_ASSERT(macroPage_);
  Q_ASSERT(actionBar_);
  Q_ASSERT(profileProvider_);

  connect(hotkeyPage_, &HotkeySettingsPage::hotkeysChanged, this,
          &HotkeyController::handleHotkeysChanged);
  connect(hotkeyPage_, &HotkeySettingsPage::activationRequested, this,
          &HotkeyController::handleActivationRequested);
  connect(service_.get(), &HotkeyService::registrationFailed, this,
          [this](const QString& message) { lastRegistrationError_ = message; });
  connect(service_.get(), &HotkeyService::startStopPressed, this,
          &HotkeyController::clickToggleRequested);
  connect(service_.get(), &HotkeyService::capturePointPressed, this,
          &HotkeyController::capturePointRequested);
  connect(service_.get(), &HotkeyService::emergencyStopPressed, this,
          &HotkeyController::emergencyStopRequested);
  connect(service_.get(), &HotkeyService::macroRecordPressed, this,
          &HotkeyController::macroRecordToggleRequested);
  connect(service_.get(), &HotkeyService::macroPlaybackPressed, this,
          &HotkeyController::macroPlaybackToggleRequested);
}

HotkeyController::~HotkeyController() = default;

void HotkeyController::initialize(const ClickProfile& profile) {
  enabled_ = false;
  actionBar_->setHotkeys(profile.hotkeys);
  hotkeyPage_->setActivationState(false, "当前未占用任何系统热键");
  macroPage_->setHotkeys(profile.hotkeys, false);
}

void HotkeyController::applyProfile(const ClickProfile& profile) {
  actionBar_->setHotkeys(profile.hotkeys);
  if (enabled_) {
    tryEnable(profile);
  } else {
    hotkeyPage_->setActivationState(false, "当前未占用任何系统热键");
    macroPage_->setHotkeys(profile.hotkeys, false);
  }
}

void HotkeyController::handleHotkeysChanged() {
  const ClickProfile profile = profileProvider_();
  QString error;
  if (!hotkeyPage_->validate(profile, &error)) {
    macroPage_->setError(error);
    if (enabled_) disable(QString("热键无效：%1").arg(error));
    return;
  }

  emit profilePersistRequested(profile);
  actionBar_->setHotkeys(profile.hotkeys);
  if (enabled_) {
    tryEnable(profile);
  } else {
    macroPage_->setHotkeys(profile.hotkeys, false);
    hotkeyPage_->setActivationState(false, "当前未占用任何系统热键");
  }
}

void HotkeyController::handleActivationRequested(bool enabled) {
  if (!enabled) {
    disable("当前未占用任何系统热键");
    return;
  }
  tryEnable(profileProvider_());
}

bool HotkeyController::tryEnable(const ClickProfile& profile) {
  QString error;
  if (!hotkeyPage_->validate(profile, &error)) {
    disable(QString("热键无效：%1").arg(error));
    macroPage_->setError(error);
    return false;
  }

  lastRegistrationError_.clear();
  enabled_ = false;
  if (!service_->registerHotkeys(profile)) {
    const QString reason = lastRegistrationError_.isEmpty()
                               ? QString("一个或多个热键不可用")
                               : lastRegistrationError_;
    disable(QString("启用失败：%1").arg(reason));
    macroPage_->setError(reason);
    return false;
  }

  enabled_ = true;
  actionBar_->setHotkeys(profile.hotkeys);
  hotkeyPage_->setActivationState(true, "全局热键已启用");
  macroPage_->setHotkeys(profile.hotkeys, true);
  macroPage_->setError({});
  emit profilePersistRequested(profile);
  return true;
}

void HotkeyController::disable(const QString& status) {
  service_->unregisterAll();
  enabled_ = false;
  hotkeyPage_->setActivationState(false, status);
  const HotkeyBindings hotkeys = profileProvider_().hotkeys;
  actionBar_->setHotkeys(hotkeys);
  macroPage_->setHotkeys(hotkeys, false);
}
