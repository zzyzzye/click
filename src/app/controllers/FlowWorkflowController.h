#pragma once
#include <QObject>
#include <QTimer>
#include <functional>
#include "core/FlowPlayer.h"
#include "core/FlowRepository.h"
#include "core/MacroController.h"
class FlowEditorPage;

class FlowWorkflowController final : public QObject {
  Q_OBJECT
public:
  FlowWorkflowController(FlowEditorPage* page, ClickBackend* backend, MacroPlayer* rawPlayer,
                         AutomationCoordinator* coordinator, QString root,
                         std::function<bool()> confirmSafety, QObject* parent = nullptr);
  bool resolveUnsaved();
  bool openRecording(const MacroSequence& sequence);
  void togglePause();
  void emergencyStop();
  void capture(bool delayed = true);
  bool isRunning() const {
    return controller_.isPlaying();
  }
  void setReservedHotkeys(std::function<QStringList()> provider) {
    reservedHotkeys_ = std::move(provider);
  }
signals:
  void runningChanged(bool running);

private:
  void run(bool trial);
  bool save();
  void refreshLibrary();
  FlowEditorPage* page_;
  ClickBackend* backend_;
  FlowRepository repository_;
  FlowPlayer player_;
  MacroController controller_;
  AutomationCoordinator* coordinator_;
  std::function<bool()> confirmSafety_;
  QTimer captureTimer_;
  std::function<QStringList()> reservedHotkeys_;
};
