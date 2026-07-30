#include "platform/windows/WindowsMacroRecorder.h"

#include <QMetaObject>
#include <QPointer>

#include <algorithm>

namespace {

bool isKeyboard(WindowsRawInputType type) {
  return type == WindowsRawInputType::KeyDown || type == WindowsRawInputType::KeyUp;
}

bool isMouseButtonDown(WindowsRawInputType type) {
  return type == WindowsRawInputType::MouseButtonDown;
}

bool isMouseButtonUp(WindowsRawInputType type) {
  return type == WindowsRawInputType::MouseButtonUp;
}

}  // namespace

WindowsMacroRecorder::WindowsMacroRecorder(
    std::unique_ptr<WindowsMacroHookApi> hookApi, WindowService* windowService,
    QObject* parent)
    : MacroRecorder(parent),
      hookApi_(std::move(hookApi)),
      windowService_(windowService) {}

WindowsMacroRecorder::~WindowsMacroRecorder() {
  stop();
}

bool WindowsMacroRecorder::start(const MacroRecordingOptions& options,
                                 QString* error) {
  if (recording_) {
    if (error) *error = "键鼠录制已在运行。";
    return false;
  }
  if (!hookApi_) {
    if (error) *error = "Windows 键鼠钩子不可用。";
    return false;
  }

  options_ = options;
  if (options_.targetMode == MacroTargetMode::Window) {
    if (!windowService_) {
      if (error) *error = "窗口录制服务不可用。";
      return false;
    }
    QString resolveError;
    const auto target = windowService_->resolve(options_.target, &resolveError);
    if (!target) {
      if (error) *error = resolveError;
      return false;
    }
    options_.target = *target;
  }

  startUs_ = hookApi_->monotonicNowUs();
  dragButtons_.clear();
  recording_ = true;
  QPointer<WindowsMacroRecorder> guard(this);
  const bool started = hookApi_->start(
      [guard](const WindowsRawInput& input) {
        if (!guard) return;
        QMetaObject::invokeMethod(
            guard,
            [guard, input] {
              if (guard) guard->handleRawInput(input);
            },
            Qt::QueuedConnection);
      },
      error);
  if (!started) {
    recording_ = false;
    return false;
  }
  return true;
}

void WindowsMacroRecorder::stop() {
  if (!recording_ && (!hookApi_ || !hookApi_->isRunning())) return;
  recording_ = false;
  dragButtons_.clear();
  if (hookApi_) hookApi_->stop();
}

bool WindowsMacroRecorder::isRecording() const {
  return recording_;
}

void WindowsMacroRecorder::handleRawInput(const WindowsRawInput& input) {
  if (!recording_ || input.extraInfo == kClickFlowInjectedInputMarker) return;

  QPoint recordedPoint = input.screenPoint;
  if (options_.targetMode == MacroTargetMode::Window) {
    if (!windowService_->isAlive(options_.target)) {
      const QString reason = "目标窗口已关闭，录制已停止。";
      stop();
      emit targetLost(reason);
      return;
    }
    if (isKeyboard(input.type)) {
      if (!windowService_->isForeground(options_.target)) return;
    } else {
      const auto relative =
          windowService_->screenToClient(options_.target, input.screenPoint);
      if (!relative) return;
      recordedPoint = *relative;
      const QSize size = windowService_->clientSize(options_.target);
      const bool inside = QRect(QPoint(0, 0), size).contains(recordedPoint);
      if (isMouseButtonDown(input.type)) {
        if (!inside) return;
        dragButtons_.insert(input.button);
      } else if (isMouseButtonUp(input.type)) {
        if (!inside && !dragButtons_.contains(input.button)) return;
      } else if (!inside && dragButtons_.isEmpty()) {
        return;
      }
    }
  }

  emit eventCaptured(convert(input, recordedPoint));
  if (isMouseButtonUp(input.type)) dragButtons_.remove(input.button);
}

MacroEvent WindowsMacroRecorder::convert(const WindowsRawInput& input,
                                         const QPoint& point) const {
  MacroEvent event;
  switch (input.type) {
    case WindowsRawInputType::KeyDown: event.type = MacroEventType::KeyDown; break;
    case WindowsRawInputType::KeyUp: event.type = MacroEventType::KeyUp; break;
    case WindowsRawInputType::MouseMove: event.type = MacroEventType::MouseMove; break;
    case WindowsRawInputType::MouseButtonDown:
      event.type = MacroEventType::MouseButtonDown;
      break;
    case WindowsRawInputType::MouseButtonUp:
      event.type = MacroEventType::MouseButtonUp;
      break;
    case WindowsRawInputType::Wheel: event.type = MacroEventType::Wheel; break;
    case WindowsRawInputType::HorizontalWheel:
      event.type = MacroEventType::HorizontalWheel;
      break;
  }
  event.offsetUs = std::max<qint64>(0, input.timestampUs - startUs_);
  event.virtualKey = input.virtualKey;
  event.scanCode = input.scanCode;
  event.nativeFlags = input.flags;
  event.point = point;
  event.button = input.button;
  event.wheelDelta = input.wheelDelta;
  return event;
}
