#include "platform/windows/WindowsWindowApi.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dwmapi.h>

#include <array>

namespace {

QString windowText(HWND window) {
  const int length = GetWindowTextLengthW(window);
  if (length <= 0) return {};
  std::wstring text(static_cast<size_t>(length) + 1, L'\0');
  const int copied = GetWindowTextW(window, text.data(), length + 1);
  return copied > 0 ? QString::fromWCharArray(text.data(), copied) : QString();
}

QString windowClass(HWND window) {
  std::array<wchar_t, 256> buffer{};
  const int copied = GetClassNameW(window, buffer.data(),
                                   static_cast<int>(buffer.size()));
  return copied > 0 ? QString::fromWCharArray(buffer.data(), copied) : QString();
}

QString executablePath(HWND window) {
  DWORD processId = 0;
  GetWindowThreadProcessId(window, &processId);
  if (!processId) return {};
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
  if (!process) return {};
  std::wstring buffer(32768, L'\0');
  DWORD size = static_cast<DWORD>(buffer.size());
  const bool succeeded =
      QueryFullProcessImageNameW(process, 0, buffer.data(), &size) != FALSE;
  CloseHandle(process);
  return succeeded ? QString::fromWCharArray(buffer.data(), static_cast<int>(size))
                   : QString();
}

WindowsNativeWindow describeWindow(HWND window) {
  WindowsNativeWindow result;
  if (!window) return result;
  result.id = reinterpret_cast<quintptr>(window);
  result.visible = IsWindowVisible(window) != FALSE;
  result.topLevel = GetAncestor(window, GA_ROOT) == window;
  BOOL cloaked = FALSE;
  if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked,
                                      sizeof(cloaked)))) {
    result.cloaked = cloaked != FALSE;
  }
  result.title = windowText(window);
  result.className = windowClass(window);
  result.executablePath = executablePath(window);
  RECT client{};
  if (GetClientRect(window, &client)) {
    result.clientSize = QSize(client.right - client.left, client.bottom - client.top);
  }
  return result;
}

class NativeWindowsWindowApi final : public WindowsWindowApi {
 public:
  QVector<WindowsNativeWindow> enumerateWindows() const override {
    QVector<WindowsNativeWindow> windows;
    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
          auto* values = reinterpret_cast<QVector<WindowsNativeWindow>*>(parameter);
          values->append(describeWindow(window));
          return TRUE;
        },
        reinterpret_cast<LPARAM>(&windows));
    return windows;
  }

  std::optional<WindowsNativeWindow> windowInfo(quintptr id) const override {
    const HWND window = reinterpret_cast<HWND>(id);
    if (!window || !IsWindow(window)) return std::nullopt;
    return describeWindow(window);
  }

  quintptr rootWindowAt(const QPoint& screenPoint) const override {
    POINT point{screenPoint.x(), screenPoint.y()};
    HWND window = WindowFromPoint(point);
    window = window ? GetAncestor(window, GA_ROOT) : nullptr;
    return reinterpret_cast<quintptr>(window);
  }

  bool isWindow(quintptr id) const override {
    return id && IsWindow(reinterpret_cast<HWND>(id));
  }

  quintptr foregroundWindow() const override {
    HWND window = GetForegroundWindow();
    window = window ? GetAncestor(window, GA_ROOT) : nullptr;
    return reinterpret_cast<quintptr>(window);
  }

  bool activateWindow(quintptr id) override {
    return id && SetForegroundWindow(reinterpret_cast<HWND>(id));
  }

  std::optional<QPoint> screenToClient(quintptr id,
                                       const QPoint& point) const override {
    POINT nativePoint{point.x(), point.y()};
    if (!id || !ScreenToClient(reinterpret_cast<HWND>(id), &nativePoint)) {
      return std::nullopt;
    }
    return QPoint(nativePoint.x, nativePoint.y);
  }

  std::optional<QPoint> clientToScreen(quintptr id,
                                       const QPoint& point) const override {
    POINT nativePoint{point.x(), point.y()};
    if (!id || !ClientToScreen(reinterpret_cast<HWND>(id), &nativePoint)) {
      return std::nullopt;
    }
    return QPoint(nativePoint.x, nativePoint.y);
  }

  QRect virtualDesktopRect() const override {
    return QRect(GetSystemMetrics(SM_XVIRTUALSCREEN),
                 GetSystemMetrics(SM_YVIRTUALSCREEN),
                 GetSystemMetrics(SM_CXVIRTUALSCREEN),
                 GetSystemMetrics(SM_CYVIRTUALSCREEN));
  }
};

}  // namespace

std::unique_ptr<WindowsWindowApi> createNativeWindowsWindowApi() {
  return std::make_unique<NativeWindowsWindowApi>();
}
