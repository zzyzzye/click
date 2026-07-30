#include "platform/windows/WindowsMacroHookApi.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {

qint64 steadyNowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

class NativeWindowsMacroHookApi final : public WindowsMacroHookApi {
 public:
  ~NativeWindowsMacroHookApi() override { stop(); }

  bool start(InputCallback callback, QString* error) override {
    if (running_.load()) {
      if (error) *error = "键鼠录制钩子已在运行。";
      return false;
    }
    callback_ = std::move(callback);
    ready_ = false;
    startSucceeded_ = false;
    worker_ = std::thread([this] { runMessageLoop(); });
    {
      std::unique_lock lock(mutex_);
      readyCondition_.wait(lock, [this] { return ready_; });
    }
    if (!startSucceeded_) {
      if (worker_.joinable()) worker_.join();
      callback_ = {};
      if (error) *error = QString("无法安装键鼠录制钩子（错误 %1）。").arg(startError_);
      return false;
    }
    running_.store(true);
    return true;
  }

  void stop() override {
    const DWORD threadId = threadId_.load();
    if (threadId) PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    if (worker_.joinable()) worker_.join();
    running_.store(false);
    threadId_.store(0);
    callback_ = {};
  }

  bool isRunning() const override { return running_.load(); }
  qint64 monotonicNowUs() const override { return steadyNowUs(); }

 private:
  static LRESULT CALLBACK keyboardProcedure(int code, WPARAM message,
                                             LPARAM parameter) {
    if (code >= 0 && active_) {
      const auto* data = reinterpret_cast<KBDLLHOOKSTRUCT*>(parameter);
      WindowsRawInput input;
      input.type = message == WM_KEYUP || message == WM_SYSKEYUP
                       ? WindowsRawInputType::KeyUp
                       : WindowsRawInputType::KeyDown;
      input.timestampUs = steadyNowUs();
      input.virtualKey = data->vkCode;
      input.scanCode = data->scanCode;
      input.flags = data->flags;
      input.extraInfo = data->dwExtraInfo;
      active_->deliver(input);
    }
    return CallNextHookEx(nullptr, code, message, parameter);
  }

  static LRESULT CALLBACK mouseProcedure(int code, WPARAM message,
                                          LPARAM parameter) {
    if (code >= 0 && active_) {
      const auto* data = reinterpret_cast<MSLLHOOKSTRUCT*>(parameter);
      WindowsRawInput input;
      input.timestampUs = steadyNowUs();
      input.screenPoint = QPoint(data->pt.x, data->pt.y);
      input.extraInfo = data->dwExtraInfo;
      bool supported = true;
      switch (message) {
        case WM_MOUSEMOVE: input.type = WindowsRawInputType::MouseMove; break;
        case WM_LBUTTONDOWN:
          input.type = WindowsRawInputType::MouseButtonDown;
          input.button = MacroMouseButton::Left;
          break;
        case WM_LBUTTONUP:
          input.type = WindowsRawInputType::MouseButtonUp;
          input.button = MacroMouseButton::Left;
          break;
        case WM_RBUTTONDOWN:
          input.type = WindowsRawInputType::MouseButtonDown;
          input.button = MacroMouseButton::Right;
          break;
        case WM_RBUTTONUP:
          input.type = WindowsRawInputType::MouseButtonUp;
          input.button = MacroMouseButton::Right;
          break;
        case WM_MBUTTONDOWN:
          input.type = WindowsRawInputType::MouseButtonDown;
          input.button = MacroMouseButton::Middle;
          break;
        case WM_MBUTTONUP:
          input.type = WindowsRawInputType::MouseButtonUp;
          input.button = MacroMouseButton::Middle;
          break;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
          input.type = message == WM_XBUTTONDOWN
                           ? WindowsRawInputType::MouseButtonDown
                           : WindowsRawInputType::MouseButtonUp;
          input.button = HIWORD(data->mouseData) == XBUTTON1
                             ? MacroMouseButton::X1
                             : MacroMouseButton::X2;
          break;
        case WM_MOUSEWHEEL:
          input.type = WindowsRawInputType::Wheel;
          input.wheelDelta = static_cast<SHORT>(HIWORD(data->mouseData));
          break;
        case WM_MOUSEHWHEEL:
          input.type = WindowsRawInputType::HorizontalWheel;
          input.wheelDelta = static_cast<SHORT>(HIWORD(data->mouseData));
          break;
        default: supported = false; break;
      }
      if (supported) active_->deliver(input);
    }
    return CallNextHookEx(nullptr, code, message, parameter);
  }

  void runMessageLoop() {
    threadId_.store(GetCurrentThreadId());
    active_ = this;
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProcedure,
                                      GetModuleHandleW(nullptr), 0);
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, mouseProcedure,
                                   GetModuleHandleW(nullptr), 0);
    {
      std::lock_guard lock(mutex_);
      startSucceeded_ = keyboardHook_ && mouseHook_;
      if (!startSucceeded_) startError_ = GetLastError();
      ready_ = true;
    }
    readyCondition_.notify_one();
    if (startSucceeded_) {
      MSG message{};
      while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
    }
    if (mouseHook_) UnhookWindowsHookEx(mouseHook_);
    if (keyboardHook_) UnhookWindowsHookEx(keyboardHook_);
    mouseHook_ = nullptr;
    keyboardHook_ = nullptr;
    active_ = nullptr;
  }

  void deliver(const WindowsRawInput& input) {
    if (callback_) callback_(input);
  }

  inline static NativeWindowsMacroHookApi* active_ = nullptr;
  InputCallback callback_;
  std::thread worker_;
  std::mutex mutex_;
  std::condition_variable readyCondition_;
  HHOOK keyboardHook_ = nullptr;
  HHOOK mouseHook_ = nullptr;
  std::atomic<DWORD> threadId_{0};
  DWORD startError_ = 0;
  bool ready_ = false;
  bool startSucceeded_ = false;
  std::atomic<bool> running_{false};
};

}  // namespace

std::unique_ptr<WindowsMacroHookApi> createNativeWindowsMacroHookApi() {
  return std::make_unique<NativeWindowsMacroHookApi>();
}
