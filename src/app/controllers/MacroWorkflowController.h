#pragma once

#include <QObject>

#include <functional>
#include <memory>
#include <optional>

#include "core/ClickTypes.h"
#include "core/MacroController.h"
#include "core/MacroRepository.h"
#include "platform/PlatformServices.h"

class AutomationCoordinator;
class MacroRecordingPage;
class StatusStrip;
class QWidget;

// 宏工作流：管理录制/回放服务、仓库、目标窗口和宏文件生命周期。
class MacroWorkflowController final : public QObject {
  Q_OBJECT

 public:
  using ProfileProvider = std::function<ClickProfile()>;
  using SafetyConfirmation = std::function<bool(QWidget*)>;
  using MacroNameProvider = std::function<QString(QWidget*)>;
  using SafetyAcknowledgedProbe = std::function<bool()>;
  using SafetyAcknowledger = std::function<void()>;
  using OwnWindowIdProvider = std::function<quintptr()>;

  MacroWorkflowController(
      MacroPlatformServices services,
      std::unique_ptr<MacroRepository> repository,
      AutomationCoordinator* automationCoordinator,
      MacroRecordingPage* macroPage, StatusStrip* statusStrip,
      ProfileProvider profileProvider,
      SafetyConfirmation safetyConfirmation,
      MacroNameProvider macroNameProvider,
      SafetyAcknowledgedProbe safetyAcknowledgedProbe,
      SafetyAcknowledger safetyAcknowledger,
      OwnWindowIdProvider ownWindowIdProvider, QWidget* dialogParent,
      QObject* parent = nullptr);

  void initialize();
  MacroControllerState state() const { return controller_.state(); }
  bool isSupported() const { return supported_; }
  void toggleRecording();
  void togglePlayback();
  void emergencyStop();

 signals:
  void stateChanged(MacroControllerState state);

 private:
  void startRecording(const MacroRecordingOptions& options);
  void startPlayback(const QString& macroId,
                     const MacroPlaybackSettings& settings);
  void handleRecordingCompleted(const MacroSequence& sequence);
  void deleteMacro(const QString& macroId);
  void renameMacro(const QString& macroId);
  void selectWindowAt(const QPoint& globalPoint);
  void handleStateChanged(MacroControllerState state);
  void refreshMacroList(const QString& selectedId = {});
  void refreshWindows(quintptr selectedNativeId = 0);
  std::optional<MacroSequence> findMacro(const QString& id) const;
  bool confirmSafety();

  MacroPlatformServices services_;
  std::unique_ptr<MacroRepository> repository_;
  MacroController controller_;
  MacroRecordingPage* macroPage_ = nullptr;
  StatusStrip* statusStrip_ = nullptr;
  ProfileProvider profileProvider_;
  SafetyConfirmation safetyConfirmation_;
  MacroNameProvider macroNameProvider_;
  SafetyAcknowledgedProbe safetyAcknowledgedProbe_;
  SafetyAcknowledger safetyAcknowledger_;
  OwnWindowIdProvider ownWindowIdProvider_;
  QWidget* dialogParent_ = nullptr;
  QVector<MacroSequence> macros_;
  bool supported_ = false;
};
