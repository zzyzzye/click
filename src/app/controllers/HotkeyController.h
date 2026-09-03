#pragma once

#include <QObject>

#include <functional>
#include <memory>

#include "core/ClickTypes.h"

class ActionBar;
class HotkeyService;
class HotkeySettingsPage;
class MacroRecordingPage;

// 热键协调器：管理系统热键注册生命周期，并将按键转换为业务意图。
class HotkeyController final : public QObject {
  Q_OBJECT

 public:
  using ProfileProvider = std::function<ClickProfile()>;

  HotkeyController(std::unique_ptr<HotkeyService> service,
                   HotkeySettingsPage* hotkeyPage,
                   MacroRecordingPage* macroPage, ActionBar* actionBar,
                   ProfileProvider profileProvider,
                   QObject* parent = nullptr);
  ~HotkeyController() override;

  void initialize(const ClickProfile& profile);
  void applyProfile(const ClickProfile& profile);
  bool isEnabled() const { return enabled_; }

 signals:
  void profilePersistRequested(const ClickProfile& profile);
  void clickToggleRequested();
  void capturePointRequested();
  void emergencyStopRequested();
  void macroRecordToggleRequested();
  void macroPlaybackToggleRequested();

 private:
  void handleHotkeysChanged();
  void handleActivationRequested(bool enabled);
  bool tryEnable(const ClickProfile& profile);
  void disable(const QString& status);

  std::unique_ptr<HotkeyService> service_;
  HotkeySettingsPage* hotkeyPage_ = nullptr;
  MacroRecordingPage* macroPage_ = nullptr;
  ActionBar* actionBar_ = nullptr;
  ProfileProvider profileProvider_;
  bool enabled_ = false;
  QString lastRegistrationError_;
};
