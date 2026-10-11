#include "app/controllers/FlowWorkflowController.h"
#include "app/pages/FlowEditorPage.h"
#include "core/MacroCompressor.h"
#include <QApplication>
#include <QAbstractButton>
#include <QMessageBox>
#include <QKeySequence>

FlowWorkflowController::FlowWorkflowController(FlowEditorPage* page, ClickBackend* backend,
                                               MacroPlayer* rawPlayer,
                                               AutomationCoordinator* coordinator, QString root,
                                               std::function<bool()> confirmSafety, QObject* parent)
    : QObject(parent), page_(page), backend_(backend), repository_(std::move(root)),
      player_(backend, rawPlayer), controller_(nullptr, &player_, coordinator, this),
      coordinator_(coordinator), confirmSafety_(std::move(confirmSafety)) {
  captureTimer_.setSingleShot(true);
  captureTimer_.setInterval(3000);
  connect(&captureTimer_, &QTimer::timeout, this, [this] {
    page_->capturePoint(backend_->currentCursorPosition(), backend_->screenRects());
    page_->setRunning(MacroControllerState::Idle);
    page_->setMessage("已捕获当前位置，请检查点位后试运行。");
  });
  connect(page_, &FlowEditorPage::newRequested, this, [this] {
    if (resolveUnsaved())
      page_->setDocument(FlowTypes::create(), false);
  });
  connect(page_, &FlowEditorPage::openRequested, this, [this](const QString& id) {
    if (id.isEmpty()) {
      page_->setMessage("请先选择已保存的流程。", true);
      return;
    }
    if (!resolveUnsaved())
      return;
    for (const auto& d : repository_.loadAll())
      if (d.id == id) {
        page_->setDocument(d);
        page_->setMessage("已打开流程。");
        return;
      }
    page_->setMessage("流程已不存在或无法读取，请重新选择。", true);
    refreshLibrary();
  });
  connect(page_, &FlowEditorPage::saveRequested, this, [this] { save(); });
  connect(page_, &FlowEditorPage::runRequested, this, &FlowWorkflowController::run);
  connect(page_, &FlowEditorPage::pauseRequested, this, &FlowWorkflowController::togglePause);
  connect(page_, &FlowEditorPage::stopRequested, this, [this] {
    if (captureTimer_.isActive())
      emergencyStop();
    else
      controller_.stop();
  });
  connect(page_, &FlowEditorPage::captureRequested, this, [this] { capture(); });
  connect(page_, &FlowEditorPage::confirmScreensRequested, this, [this] {
    if (backend_->screenRects().isEmpty()) {
      page_->setMessage("无法读取显示器布局，请检查系统输入支持。", true);
      return;
    }
    page_->confirmScreens(backend_->screenRects());
    page_->setMessage("已确认当前布局；运行前请检查每个坐标。");
  });
  connect(&controller_, &MacroController::stateChanged, this, [this](MacroControllerState state) {
    page_->setRunning(state);
    emit runningChanged(controller_.isPlaying());
  });
  connect(&controller_, &MacroController::stepProgress, page_, &FlowEditorPage::setProgress);
  connect(&controller_, &MacroController::statusChanged, this, [this](const QString& s) {
    QString text = s;
    text.replace("宏回放", "流程");
    page_->setMessage(text);
    if (s.startsWith("等待当前完整动作"))
      page_->setPausePending();
  });
  connect(&controller_, &MacroController::failed, this,
          [this](const QString& reason) { page_->setMessage(reason, true); });
  connect(coordinator, &AutomationCoordinator::emergencyStopRequested, this,
          &FlowWorkflowController::emergencyStop);
  connect(coordinator, &AutomationCoordinator::activityChanged, this,
          [this](AutomationActivity activity) {
            if (!controller_.isPlaying()) {
              if (activity != AutomationActivity::Idle)
                captureTimer_.stop();
              page_->setRunning(MacroControllerState::Idle, activity != AutomationActivity::Idle);
            }
          });
  refreshLibrary();
}
void FlowWorkflowController::refreshLibrary() {
  QStringList warnings;
  page_->setLibrary(repository_.loadAll(&warnings));
  if (!warnings.isEmpty()) {
    QString message = warnings.mid(0, 2).join('\n');
    if (warnings.size() > 2)
      message +=
          QString("\n另有 %1 个文件无法读取，请检查本机 flows 目录。").arg(warnings.size() - 2);
    page_->setMessage(message, true);
  }
}
bool FlowWorkflowController::save() {
  if (coordinator_->activity() != AutomationActivity::Idle)
    return false;
  QString error;
  int invalid = -1;
  if (!FlowTypes::validate(page_->document(), &error, &invalid) ||
      !repository_.save(page_->document(), &error)) {
    page_->focusInvalidStep(invalid);
    page_->setMessage(error, true);
    return false;
  }
  page_->markSaved();
  refreshLibrary();
  page_->setMessage("流程已保存到本机。");
  return true;
}
bool FlowWorkflowController::resolveUnsaved() {
  if (!page_->isDirty())
    return true;
  QMessageBox prompt(QMessageBox::Question, "流程尚未保存",
                     "是否保存当前流程？取消将保留编辑内容。",
                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, page_);
  prompt.button(QMessageBox::Save)->setText("保存流程");
  prompt.button(QMessageBox::Discard)->setText("放弃修改");
  prompt.button(QMessageBox::Cancel)->setText("取消");
  prompt.setDefaultButton(QMessageBox::Cancel);
  const int result = prompt.exec();
  if (result == QMessageBox::Save)
    return save();
  return result == QMessageBox::Discard;
}
void FlowWorkflowController::run(bool trial) {
  if (QApplication::activeModalWidget() || captureTimer_.isActive())
    return;
  if (coordinator_->activity() != AutomationActivity::Idle) {
    page_->setMessage("另一个自动化任务正在运行，请先停止。", true);
    return;
  }
  QString error;
  int invalid = -1;
  if (!FlowTypes::validate(page_->document(), &error, &invalid)) {
    page_->focusInvalidStep(invalid);
    page_->setMessage(error, true);
    return;
  }
  if (reservedHotkeys_) {
    const auto reserved = reservedHotkeys_();
    for (int i = 0; i < page_->document().steps.size(); ++i) {
      const auto& s = page_->document().steps[i];
      if (s.kind == FlowStepKind::Recording &&
          MacroCompressor::containsReservedKey(s.recording.events, reserved)) {
        page_->focusInvalidStep(i);
        page_->setMessage(
            QString("步骤 %1 的录制包含已启用的控制热键主键，请更换热键或关闭全局热键后试运行。")
                .arg(i + 1),
            true);
        return;
      }
      if (s.kind != FlowStepKind::Key)
        continue;
      const auto key = QKeySequence::fromString(s.value, QKeySequence::PortableText);
      for (const auto& binding : reserved) {
        const auto hotkey = QKeySequence::fromString(binding, QKeySequence::PortableText);
        if (key.count() == 1 && hotkey.count() == 1 && key[0].key() == hotkey[0].key()) {
          page_->focusInvalidStep(i);
          page_->setMessage(
              QString("步骤 %1 的按键与已启用的控制热键冲突，请更换按键或热键。").arg(i + 1), true);
          return;
        }
      }
    }
  }
  if (!confirmSafety_())
    return;
  auto snapshot = page_->document();
  if (trial) {
    snapshot.playback.infinite = false;
    snapshot.playback.repeatCount = 1;
    snapshot.playback.loopDelayMs = 0;
  }
  auto sequence = FlowTypes::compile(snapshot, &error);
  if (!sequence) {
    page_->setMessage(error, true);
    return;
  }
  player_.setDocument(snapshot);
  page_->setRunOptions(snapshot.playback);
  page_->setMessage({});
  if (!controller_.startPlayback(*sequence, &error)) {
    page_->focusInvalidStep(player_.invalidStep());
    page_->setMessage(error, true);
  }
}
void FlowWorkflowController::togglePause() {
  if (QApplication::activeModalWidget())
    return;
  if (controller_.isPaused()) {
    QString error;
    if (!controller_.resume(&error))
      page_->setMessage(error, true);
  } else
    controller_.pause();
}
void FlowWorkflowController::emergencyStop() {
  if (captureTimer_.isActive()) {
    captureTimer_.stop();
    page_->setRunning(MacroControllerState::Idle);
    page_->setMessage("坐标捕获已取消。");
  }
  controller_.emergencyStop();
}
void FlowWorkflowController::capture(bool delayed) {
  if (QApplication::activeModalWidget() || coordinator_->activity() != AutomationActivity::Idle ||
      captureTimer_.isActive())
    return;
  if (!delayed) {
    page_->capturePoint(backend_->currentCursorPosition(), backend_->screenRects());
    return;
  }
  page_->setMessage("3 秒内将鼠标移到目标位置；按紧急停止可取消。");
  page_->setCapturing();
  captureTimer_.start();
}
bool FlowWorkflowController::openRecording(const MacroSequence& sequence) {
  if (coordinator_->activity() != AutomationActivity::Idle || !resolveUnsaved())
    return false;
  auto d = FlowTypes::create();
  d.name = sequence.name + "（步骤流程）";
  d.playback = sequence.playback;
  d.playback.speed = 1.0;
  d.screens = backend_->screenRects();
  page_->setDocument(d, false);
  page_->addRecording(sequence);
  page_->setMessage("原录制文件保持不变，编辑后保存为独立流程。录制块保留原回放速度。");
  return true;
}
