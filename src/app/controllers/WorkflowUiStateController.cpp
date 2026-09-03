#include "app/controllers/WorkflowUiStateController.h"

#include "app/pages/ClickSettingsPage.h"
#include "app/pages/HotkeySettingsPage.h"
#include "app/pages/MacroRecordingPage.h"
#include "app/pages/PresetsAboutPage.h"
#include "app/widgets/ActionBar.h"

WorkflowUiStateController::WorkflowUiStateController(
    ClickSettingsPage* clickPage, HotkeySettingsPage* hotkeyPage,
    MacroRecordingPage* macroPage, PresetsAboutPage* presetsPage,
    ActionBar* actionBar, bool macroSupported, QObject* parent)
    : QObject(parent),
      clickPage_(clickPage),
      hotkeyPage_(hotkeyPage),
      macroPage_(macroPage),
      presetsPage_(presetsPage),
      actionBar_(actionBar),
      macroSupported_(macroSupported) {
  Q_ASSERT(clickPage_);
  Q_ASSERT(hotkeyPage_);
  Q_ASSERT(macroPage_);
  Q_ASSERT(presetsPage_);
  Q_ASSERT(actionBar_);
  apply();
}

void WorkflowUiStateController::setClickRunning(bool running) {
  clickRunning_ = running;
  apply();
}

void WorkflowUiStateController::setMacroState(MacroControllerState state) {
  macroState_ = state;
  apply();
}

void WorkflowUiStateController::apply() {
  const bool macroIdle = macroState_ == MacroControllerState::Idle;
  const bool editingEnabled = !clickRunning_ && macroIdle;
  clickPage_->setEditingEnabled(editingEnabled);
  hotkeyPage_->setEditingEnabled(editingEnabled);
  presetsPage_->setMutationEnabled(editingEnabled);
  actionBar_->setEnabled(macroIdle);

  if (!macroSupported_) {
    macroPage_->setActivity(MacroPageActivity::Unavailable);
    return;
  }
  if (clickRunning_) {
    macroPage_->setActivity(MacroPageActivity::Unavailable);
    macroPage_->setError("连点运行中，键鼠录制与回放暂不可用。");
    return;
  }
  if (macroState_ == MacroControllerState::Recording) {
    macroPage_->setActivity(MacroPageActivity::Recording);
  } else if (macroState_ == MacroControllerState::Playing) {
    macroPage_->setActivity(MacroPageActivity::Playing);
  } else {
    macroPage_->setActivity(MacroPageActivity::Idle);
  }
}
