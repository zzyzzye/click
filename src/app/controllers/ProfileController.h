#pragma once

#include <QObject>

#include <memory>

#include "core/ClickTypes.h"

class ActionBar;
class ClickSettingsPage;
class HotkeySettingsPage;
class PresetsAboutPage;
class SettingsRepository;
class QWidget;
class WindowChromeController;

// 配置协调器：统一管理配置持久化、页面投影和预设生命周期。
class ProfileController final : public QObject {
  Q_OBJECT

 public:
  ProfileController(std::unique_ptr<SettingsRepository> repository,
                    ClickSettingsPage* clickPage,
                    HotkeySettingsPage* hotkeyPage,
                    PresetsAboutPage* presetsPage, ActionBar* actionBar,
                    WindowChromeController* windowChrome, QWidget* dialogParent,
                    QObject* parent = nullptr);
  ~ProfileController() override;

  void initialize();
  ClickProfile currentProfile() const;
  void saveLastUsed(const ClickProfile& profile);
  bool macroSafetyAcknowledged() const;
  void acknowledgeMacroSafety();

 signals:
  void profileApplied(const ClickProfile& profile);
  void statusChanged(const QString& status);

 private:
  void applyProfile(const ClickProfile& profile);
  void refreshPresetList(const QString& selected = {});
  void savePreset();
  void createPreset();
  void renamePreset();
  void deletePreset();
  void loadPreset();

  std::unique_ptr<SettingsRepository> repository_;
  ClickSettingsPage* clickPage_ = nullptr;
  HotkeySettingsPage* hotkeyPage_ = nullptr;
  PresetsAboutPage* presetsPage_ = nullptr;
  ActionBar* actionBar_ = nullptr;
  WindowChromeController* windowChrome_ = nullptr;
  QWidget* dialogParent_ = nullptr;
  QString currentProfileName_ = "Default";
  bool applyingProfile_ = false;
};
