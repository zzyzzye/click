#include "platform/windows/WindowsWindowStyle.h"

#include <QDebug>
#include <QString>
#include <QWidget>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dwmapi.h>

namespace {

// 若 SDK 头未提供这些常量则回退硬编码（MSVC 2022 + 新 SDK 通常已有）。
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

constexpr int kDwmwcpRound = 2;       // DWMWCP_ROUND
constexpr int kDwmsbtMainWindow = 2;  // DWMSBT_MAINWINDOW（Mica）

unsigned int currentBuildNumber() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                    KEY_READ, &key) != ERROR_SUCCESS) {
    return 0;
  }
  wchar_t buffer[32] = {};
  DWORD size = sizeof(buffer);
  const LSTATUS status =
      RegQueryValueExW(key, L"CurrentBuildNumber", nullptr, nullptr,
                       reinterpret_cast<LPBYTE>(buffer), &size);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS) return 0;
  bool ok = false;
  const unsigned int build =
      QString::fromWCharArray(buffer).toUInt(&ok);
  return ok ? build : 0;
}

// 系统「设置 → 个性化 → 颜色 → 透明效果」。关闭时 Windows 不绘制 Mica。
bool readTransparencyEnabled() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0, KEY_READ, &key) != ERROR_SUCCESS) {
    return true;  // 默认开启
  }
  DWORD value = 1;
  DWORD size = sizeof(value);
  RegQueryValueExW(key, L"EnableTransparency", nullptr, nullptr,
                   reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(key);
  return value != 0;
}

HWND widgetHandle(const QWidget* widget) {
  return reinterpret_cast<HWND>(widget->winId());
}

void applyNativeWindowStyle(HWND handle) {
  const auto current = static_cast<quintptr>(GetWindowLongPtrW(handle, GWL_STYLE));
  const auto desired = clickFlowNativeWindowStyle(current);
  if (desired == current) return;

  SetLastError(ERROR_SUCCESS);
  const LONG_PTR previous = SetWindowLongPtrW(
      handle, GWL_STYLE, static_cast<LONG_PTR>(desired));
  if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
    qWarning() << "Windows 窗口样式更新失败，错误码 =" << GetLastError();
    return;
  }

  SetWindowPos(handle, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);
}

void applyDwmAttribute(HWND handle, DWORD attribute, const void* value,
                       DWORD size, const char* description) {
  const HRESULT result = DwmSetWindowAttribute(handle, attribute, value, size);
  if (FAILED(result)) {
    qWarning() << "DWM 设置失败:" << description
               << "HRESULT =" << Qt::hex << result;
  }
}

}  // namespace

bool isWindows11OrLater(unsigned int buildNumber) {
  return buildNumber >= 22000;
}

bool supportsSystemBackdrop(unsigned int buildNumber) {
  return buildNumber >= 22621;
}

bool micaBackdropAvailable(unsigned int buildNumber, bool transparencyEnabled) {
  return isWindows11OrLater(buildNumber) &&
         supportsSystemBackdrop(buildNumber) && transparencyEnabled;
}

quintptr clickFlowNativeWindowStyle(quintptr currentStyle) {
  // DWM 在部分 Windows 11 版本上即使 WS_CAPTION 已清除，只要 WS_SYSMENU
  // 或原生最小化按钮位仍存在，仍会合成一条幽灵标题栏。保留缩放边框和
  // 最大化能力，其余窗口按钮全部交给 CaptionBar。
  currentStyle &= ~static_cast<quintptr>(
      WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
  currentStyle |= static_cast<quintptr>(WS_THICKFRAME | WS_MAXIMIZEBOX);
  return currentStyle;
}

bool WindowsWindowStyle::usesBackdrop() const {
  return micaBackdropAvailable(currentBuildNumber(), readTransparencyEnabled());
}

void WindowsWindowStyle::prepare(QWidget* window) {
  if (!window) return;
  // 仅在 Mica 确定可用时才透明化窗口，否则透明区域会直接透出桌面。
  if (usesBackdrop()) {
    window->setAttribute(Qt::WA_TranslucentBackground);
  }
}

void WindowsWindowStyle::apply(QWidget* window) {
  if (!window) return;

  HWND handle = widgetHandle(window);
  if (!handle) return;

  applyNativeWindowStyle(handle);
  if (!usesBackdrop()) return;

  const int corner = kDwmwcpRound;
  applyDwmAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                    sizeof(corner), "窗口圆角");

  const int backdrop = kDwmsbtMainWindow;
  applyDwmAttribute(handle, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop,
                    sizeof(backdrop), "Mica 背景");

  // 仅浅色主题：始终关闭沉浸式暗色标题栏。
  const BOOL dark = FALSE;
  applyDwmAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark),
                    "暗色标题栏");

  // 将 DWM 框架扩展到整个客户区，确保透明区域绘制 Mica 材质而非透出桌面。
  const MARGINS margins{-1, -1, -1, -1};
  const HRESULT marginsResult = DwmExtendFrameIntoClientArea(handle, &margins);
  if (FAILED(marginsResult)) {
    qWarning() << "DWM 框架扩展失败: HRESULT =" << Qt::hex << marginsResult;
  }
}
