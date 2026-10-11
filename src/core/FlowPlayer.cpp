#include "core/FlowPlayer.h"
#include <algorithm>

FlowPlayer::FlowPlayer(ClickBackend* backend, MacroPlayer* rawPlayer)
    : backend_(backend), raw_(rawPlayer) {}
void FlowPlayer::setDocument(const FlowDocument& d) {
  document_ = d;
  hasPoints_ = false;
  for (const auto& s : d.steps) {
    hasPoints_ |= s.kind == FlowStepKind::Click;
    if (s.kind == FlowStepKind::Recording && s.recording.targetMode == MacroTargetMode::Global)
      for (const auto& e : s.recording.events)
        hasPoints_ |=
            e.type >= MacroEventType::MouseMove && e.type <= MacroEventType::HorizontalWheel;
  }
}
bool FlowPlayer::revalidate(QString* error) {
  invalidStep_ = -1;
  auto fail = [&](const QString& s, int step = -1) {
    invalidStep_ = step;
    if (error)
      *error = s;
    return false;
  };
  if (!backend_ || !backend_->hasAccessibilityPermission())
    return fail("输入权限不可用，请在系统设置中允许 ClickFlow 控制输入；Linux 需要 X11。");
  const auto screens = backend_->screenRects();
  bool points = false;
  for (int i = 0; i < document_.steps.size(); ++i) {
    const auto& step = document_.steps[i];
    if (step.kind == FlowStepKind::Click) {
      points = true;
      if (std::none_of(screens.begin(), screens.end(),
                       [&](const QRect& r) { return r.contains(step.point); }))
        return fail(QString("步骤 %1：坐标不在当前屏幕内，请重新捕获位置。").arg(i + 1), i);
    }
    if (step.kind == FlowStepKind::Key && !backend_->supportsKey(step.value))
      return fail(QString("步骤 %1：当前平台不支持这个按键组合，请更换按键。").arg(i + 1), i);
    if (step.kind == FlowStepKind::Text && !backend_->supportsText())
      return fail(QString("步骤 %1：此平台尚不支持文字步骤；Linux 可先使用按键步骤。").arg(i + 1),
                  i);
    if (step.kind == FlowStepKind::Recording && !raw_)
      return fail(
          QString("步骤 %1：原始录制回放仅支持 Windows；可创建点击、按键和等待步骤。").arg(i + 1),
          i);
    if (step.kind == FlowStepKind::Recording &&
        step.recording.targetMode == MacroTargetMode::Global) {
      for (const auto& e : step.recording.events) {
        if (e.type < MacroEventType::MouseMove || e.type > MacroEventType::HorizontalWheel)
          continue;
        points = true;
        if (std::none_of(screens.begin(), screens.end(),
                         [&](const QRect& r) { return r.contains(e.point); }))
          return fail(QString("步骤 %1：录制坐标不在当前屏幕内，请恢复布局或重新录制。").arg(i + 1),
                      i);
      }
    }
  }
  if (points && (document_.screens.isEmpty() || screens != document_.screens))
    return fail("显示器布局与编辑时不同，请检查所有点位后点击“确认当前屏幕布局”。");
  return !rawPrepared_ || raw_->revalidate(error);
}
bool FlowPlayer::prepare(const MacroSequence& sequence, QString* error) {
  cancel();
  if (!revalidate(error))
    return false;
  std::optional<MacroSequence> rawSequence;
  for (const auto& step : document_.steps) {
    if (step.kind != FlowStepKind::Recording)
      continue;
    if (!raw_) {
      if (error)
        *error = "原始录制回放仅支持 Windows；可创建点击、按键和等待步骤。";
      return false;
    }
    if (!rawSequence)
      rawSequence = step.recording;
    else if (rawSequence->targetMode != step.recording.targetMode ||
             (rawSequence->targetMode == MacroTargetMode::Window &&
              (rawSequence->target.executablePath != step.recording.target.executablePath ||
               rawSequence->target.className != step.recording.target.className ||
               rawSequence->target.title != step.recording.target.title ||
               rawSequence->target.clientSize != step.recording.target.clientSize))) {
      if (error)
        *error = "同一流程中的录制块必须使用相同目标窗口或全部使用全局模式。";
      return false;
    }
  }
  if (rawSequence) {
    if (rawSequence->targetMode == MacroTargetMode::Window) {
      for (const auto& s : document_.steps)
        if (s.kind != FlowStepKind::Recording && s.kind != FlowStepKind::Wait) {
          if (error)
            *error = "绑定窗口的录制块目前只可搭配等待步骤，避免混用屏幕与客户区坐标。";
          return false;
        }
    }
    rawSequence->events.clear();
    for (const auto& e : sequence.events)
      if (int(e.type) <= int(MacroEventType::HorizontalWheel))
        rawSequence->events.append(e);
    rawSequence->durationUs = sequence.durationUs;
    if (!raw_->prepare(*rawSequence, error)) {
      raw_->cancel();
      return false;
    }
    rawPrepared_ = true;
  }
  lastStep_ = -1;
  checkClock_.start();
  return true;
}
bool FlowPlayer::inject(const MacroEvent& e, QString* error) {
  // 按步骤或每 250ms 检查运行条件，避免每个原始事件遍历整个流程。
  if (e.stepIndex != lastStep_ || !checkClock_.isValid() || checkClock_.elapsed() >= 250) {
    if (!backend_->hasAccessibilityPermission() ||
        (hasPoints_ && backend_->screenRects() != document_.screens)) {
      if (error)
        *error = "输入权限或屏幕布局已变化，请重新检查后从头运行。";
      return false;
    }
    lastStep_ = e.stepIndex;
    checkClock_.restart();
  }
  ClickProfile p;
  p.targetMode = TargetMode::FixedPoint;
  p.fixedPoint = e.point;
  p.button = e.button == MacroMouseButton::Right ? ClickButton::Right : ClickButton::Left;
  bool ok = false;
  switch (e.type) {
  case MacroEventType::Boundary:
    releaseAll();
    return true;
  case MacroEventType::ActionClick:
    ok = backend_->click(p);
    break;
  case MacroEventType::ActionKey:
    p.keyboardKey = e.text;
    ok = backend_->keyTap(p);
    break;
  case MacroEventType::ActionText:
    ok = backend_->textInput(e.text);
    break;
  default:
    return rawPrepared_ && raw_->inject(e, error);
  }
  if (!ok && error)
    *error = "输入未成功发送，请检查权限及目标程序后重新运行。";
  return ok;
}
void FlowPlayer::releaseAll() {
  if (rawPrepared_)
    raw_->releaseAll();
}
void FlowPlayer::cancel() {
  if (rawPrepared_)
    raw_->cancel();
  rawPrepared_ = false;
}
