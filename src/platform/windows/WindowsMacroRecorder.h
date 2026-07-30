#pragma once

#include <QSet>

#include <memory>

#include "core/MacroRecorder.h"
#include "core/WindowService.h"
#include "platform/windows/WindowsMacroHookApi.h"

class WindowsMacroRecorder final : public MacroRecorder {
  Q_OBJECT

 public:
  explicit WindowsMacroRecorder(
      std::unique_ptr<WindowsMacroHookApi> hookApi = createNativeWindowsMacroHookApi(),
      WindowService* windowService = nullptr, QObject* parent = nullptr);
  ~WindowsMacroRecorder() override;

  bool start(const MacroRecordingOptions& options,
             QString* error = nullptr) override;
  void stop() override;
  bool isRecording() const override;

 private:
  void handleRawInput(const WindowsRawInput& input);
  MacroEvent convert(const WindowsRawInput& input, const QPoint& point) const;

  std::unique_ptr<WindowsMacroHookApi> hookApi_;
  WindowService* windowService_ = nullptr;
  MacroRecordingOptions options_;
  QSet<MacroMouseButton> dragButtons_;
  qint64 startUs_ = 0;
  bool recording_ = false;
};
