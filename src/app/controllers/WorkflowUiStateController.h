#pragma once

#include <QObject>

#include "core/MacroController.h"

class ActionBar;
class ClickSettingsPage;
class HotkeySettingsPage;
class MacroRecordingPage;
class PresetsAboutPage;

// 跨工作流 UI 状态机：唯一负责页面编辑锁定和宏页面活动状态。
class WorkflowUiStateController final : public QObject {
  Q_OBJECT

 public:
  WorkflowUiStateController(ClickSettingsPage* clickPage,
                            HotkeySettingsPage* hotkeyPage,
                            MacroRecordingPage* macroPage,
                            PresetsAboutPage* presetsPage,
                            ActionBar* actionBar, bool macroSupported,
                            QObject* parent = nullptr);

 public slots:
  void setClickRunning(bool running);
  void setMacroState(MacroControllerState state);

 private:
  void apply();

  ClickSettingsPage* clickPage_ = nullptr;
  HotkeySettingsPage* hotkeyPage_ = nullptr;
  MacroRecordingPage* macroPage_ = nullptr;
  PresetsAboutPage* presetsPage_ = nullptr;
  ActionBar* actionBar_ = nullptr;
  bool macroSupported_ = false;
  bool clickRunning_ = false;
  MacroControllerState macroState_ = MacroControllerState::Idle;
};
