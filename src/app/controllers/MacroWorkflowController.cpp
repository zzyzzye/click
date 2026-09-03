#include "app/controllers/MacroWorkflowController.h"

#include <QDateTime>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QWidget>

#include <algorithm>

#include "app/pages/MacroRecordingPage.h"
#include "app/widgets/StatusStrip.h"
#include "core/AutomationCoordinator.h"

MacroWorkflowController::MacroWorkflowController(
    MacroPlatformServices services,
    std::unique_ptr<MacroRepository> repository,
    AutomationCoordinator* automationCoordinator,
    MacroRecordingPage* macroPage, StatusStrip* statusStrip,
    ProfileProvider profileProvider, SafetyConfirmation safetyConfirmation,
    MacroNameProvider macroNameProvider,
    SafetyAcknowledgedProbe safetyAcknowledgedProbe,
    SafetyAcknowledger safetyAcknowledger,
    OwnWindowIdProvider ownWindowIdProvider, QWidget* dialogParent,
    QObject* parent)
    : QObject(parent),
      services_(std::move(services)),
      repository_(std::move(repository)),
      controller_(services_.recorder.get(), services_.player.get(),
                  automationCoordinator, this),
      macroPage_(macroPage),
      statusStrip_(statusStrip),
      profileProvider_(std::move(profileProvider)),
      safetyConfirmation_(std::move(safetyConfirmation)),
      macroNameProvider_(std::move(macroNameProvider)),
      safetyAcknowledgedProbe_(std::move(safetyAcknowledgedProbe)),
      safetyAcknowledger_(std::move(safetyAcknowledger)),
      ownWindowIdProvider_(std::move(ownWindowIdProvider)),
      dialogParent_(dialogParent),
      supported_(services_.windowService && services_.recorder &&
                 services_.player && repository_) {
  Q_ASSERT(automationCoordinator);
  Q_ASSERT(macroPage_);
  Q_ASSERT(statusStrip_);
  Q_ASSERT(profileProvider_);
  Q_ASSERT(safetyConfirmation_);
  Q_ASSERT(macroNameProvider_);
  Q_ASSERT(safetyAcknowledgedProbe_);
  Q_ASSERT(safetyAcknowledger_);
  Q_ASSERT(ownWindowIdProvider_);
  Q_ASSERT(dialogParent_);

  connect(macroPage_, &MacroRecordingPage::recordRequested, this,
          &MacroWorkflowController::startRecording);
  connect(macroPage_, &MacroRecordingPage::playRequested, this,
          &MacroWorkflowController::startPlayback);
  connect(macroPage_, &MacroRecordingPage::stopRequested, &controller_,
          &MacroController::stop);
  connect(macroPage_, &MacroRecordingPage::deleteRequested, this,
          &MacroWorkflowController::deleteMacro);
  connect(macroPage_, &MacroRecordingPage::renameRequested, this,
          &MacroWorkflowController::renameMacro);
  connect(macroPage_, &MacroRecordingPage::refreshWindowsRequested, this,
          [this] { refreshWindows(); });
  connect(macroPage_, &MacroRecordingPage::windowPointSelected, this,
          &MacroWorkflowController::selectWindowAt);

  connect(&controller_, &MacroController::stateChanged, this,
          &MacroWorkflowController::handleStateChanged);
  connect(&controller_, &MacroController::recordingProgress, macroPage_,
          &MacroRecordingPage::setRecordingProgress);
  connect(&controller_, &MacroController::playbackProgress, macroPage_,
          &MacroRecordingPage::setPlaybackProgress);
  connect(&controller_, &MacroController::recordingCompleted, this,
          &MacroWorkflowController::handleRecordingCompleted);
  connect(&controller_, &MacroController::statusChanged, statusStrip_,
          &StatusStrip::setStatus);
  connect(&controller_, &MacroController::failed, this,
          [this](const QString& reason) {
            macroPage_->setError(reason);
            statusStrip_->setStatus(reason);
          });
}

void MacroWorkflowController::initialize() {
  macroPage_->setSupported(
      supported_, supported_ ? QString()
                             : "键鼠录制当前仅支持 Windows 10/11。");
  refreshMacroList();
  refreshWindows();
}

void MacroWorkflowController::toggleRecording() {
  if (controller_.isRecording()) {
    controller_.stop();
  } else {
    startRecording(macroPage_->recordingOptions());
  }
}

void MacroWorkflowController::togglePlayback() {
  if (controller_.isPlaying()) {
    controller_.stop();
  } else {
    startPlayback(macroPage_->selectedMacroId(),
                  macroPage_->playbackSettings());
  }
}

void MacroWorkflowController::emergencyStop() {
  controller_.emergencyStop();
}

void MacroWorkflowController::startRecording(
    const MacroRecordingOptions& options) {
  if (!supported_) {
    macroPage_->setError("键鼠录制服务不可用。");
    return;
  }
  if (options.targetMode == MacroTargetMode::Window &&
      !options.target.nativeId) {
    macroPage_->setError("请先选择一个目标窗口。");
    return;
  }
  if (!confirmSafety()) return;

  QString error;
  if (!controller_.startRecording(options, &error)) {
    macroPage_->setError(error);
    statusStrip_->setStatus(error);
  } else {
    macroPage_->setError({});
  }
}

void MacroWorkflowController::startPlayback(
    const QString& macroId, const MacroPlaybackSettings& settings) {
  const auto saved = findMacro(macroId);
  if (!saved) {
    macroPage_->setError("请先选择一个已保存的宏。");
    return;
  }
  MacroSequence sequence = *saved;
  sequence.playback = settings;
  sequence.modifiedAt = QDateTime::currentDateTimeUtc();
  QString error;
  if (!repository_->save(sequence, &error) ||
      !controller_.startPlayback(sequence, &error)) {
    macroPage_->setError(error);
    statusStrip_->setStatus(error);
  } else {
    macroPage_->setError({});
  }
}

void MacroWorkflowController::handleRecordingCompleted(
    const MacroSequence& sequence) {
  if (!repository_) return;
  MacroSequence saved = sequence;
  saved.name = macroNameProvider_(dialogParent_).trimmed();
  if (saved.name.isEmpty()) {
    statusStrip_->setStatus("录制未保存");
    return;
  }
  saved.modifiedAt = QDateTime::currentDateTimeUtc();
  QString error;
  if (!repository_->save(saved, &error)) {
    macroPage_->setError(error);
    return;
  }
  refreshMacroList(saved.id);
  statusStrip_->setStatus("宏已保存");
}

void MacroWorkflowController::deleteMacro(const QString& macroId) {
  const auto sequence = findMacro(macroId);
  if (!sequence || !repository_) return;
  if (QMessageBox::question(
          dialogParent_, "删除宏",
          QString("确定删除“%1”吗？").arg(sequence->name)) !=
      QMessageBox::Yes) {
    return;
  }
  QString error;
  if (!repository_->remove(macroId, &error)) macroPage_->setError(error);
  refreshMacroList();
}

void MacroWorkflowController::renameMacro(const QString& macroId) {
  const auto sequence = findMacro(macroId);
  if (!sequence || !repository_) return;
  bool accepted = false;
  const QString name =
      QInputDialog::getText(dialogParent_, "重命名宏", "新名称",
                            QLineEdit::Normal, sequence->name, &accepted)
          .trimmed();
  if (!accepted || name.isEmpty()) return;
  QString error;
  if (!repository_->rename(macroId, name, &error)) {
    macroPage_->setError(error);
  }
  refreshMacroList(macroId);
}

void MacroWorkflowController::selectWindowAt(const QPoint& globalPoint) {
  if (!services_.windowService) return;
  const auto target = services_.windowService->windowAt(globalPoint);
  if (!target || target->nativeId == ownWindowIdProvider_()) {
    macroPage_->setError("没有选中可录制的目标窗口。");
    return;
  }
  refreshWindows(target->nativeId);
}

void MacroWorkflowController::handleStateChanged(MacroControllerState state) {
  const ClickProfile profile = profileProvider_();
  if (state == MacroControllerState::Recording) {
    statusStrip_->setProgress(
        QString("%1 停止 · %2 紧急停止")
            .arg(profile.hotkeys.macroRecord,
                 profile.hotkeys.emergencyStop));
  } else if (state == MacroControllerState::Playing) {
    statusStrip_->setProgress(
        QString("%1 停止 · %2 紧急停止")
            .arg(profile.hotkeys.macroPlayback,
                 profile.hotkeys.emergencyStop));
  } else {
    statusStrip_->setProgress("就绪");
  }
  emit stateChanged(state);
}

void MacroWorkflowController::refreshMacroList(const QString& selectedId) {
  if (!repository_) {
    macros_.clear();
    macroPage_->setMacros({});
    return;
  }
  QStringList warnings;
  macros_ = repository_->loadAll(&warnings);
  macroPage_->setMacros(macros_, selectedId);
  if (!warnings.isEmpty()) macroPage_->setError(warnings.first());
}

void MacroWorkflowController::refreshWindows(quintptr selectedNativeId) {
  if (!services_.windowService) {
    macroPage_->setAvailableWindows({});
    return;
  }
  QVector<WindowTarget> windows = services_.windowService->availableWindows();
  const quintptr ownWindow = ownWindowIdProvider_();
  windows.erase(std::remove_if(
                    windows.begin(), windows.end(),
                    [ownWindow](const WindowTarget& target) {
                      return target.nativeId == ownWindow;
                    }),
                windows.end());
  macroPage_->setAvailableWindows(windows, selectedNativeId);
}

std::optional<MacroSequence> MacroWorkflowController::findMacro(
    const QString& id) const {
  const auto match =
      std::find_if(macros_.cbegin(), macros_.cend(),
                   [&id](const MacroSequence& sequence) {
                     return sequence.id == id;
                   });
  return match == macros_.cend() ? std::nullopt
                                 : std::optional<MacroSequence>(*match);
}

bool MacroWorkflowController::confirmSafety() {
  if (safetyAcknowledgedProbe_()) return true;
  if (!safetyConfirmation_(dialogParent_)) return false;
  safetyAcknowledger_();
  return true;
}
