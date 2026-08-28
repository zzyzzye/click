#include "platform/windows/WindowsWindowStyle.h"

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

bool readAppsUseLightTheme() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0, KEY_READ, &key) != ERROR_SUCCESS) {
    return true;  // 默认亮色
  }
  DWORD value = 1;
  DWORD size = sizeof(value);
  RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                   reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(key);
  return value != 0;
}

HWND widgetHandle(const QWidget* widget) {
  return reinterpret_cast<HWND>(widget->winId());
}

}  // namespace

bool isWindows11OrLater(unsigned int buildNumber) {
  return buildNumber >= 22000;
}

void WindowsWindowStyle::prepare(QWidget* window) {
  if (!window) return;
  if (isWindows11OrLater(currentBuildNumber())) {
    window->setAttribute(Qt::WA_TranslucentBackground);
  }
}

void WindowsWindowStyle::apply(QWidget* window) {
  if (!window) return;
  if (!isWindows11OrLater(currentBuildNumber())) return;

  HWND handle = widgetHandle(window);
  if (!handle) return;

  const int corner = kDwmwcpRound;
  DwmSetWindowAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                        sizeof(corner));

  const int backdrop = kDwmsbtMainWindow;
  DwmSetWindowAttribute(handle, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop,
                        sizeof(backdrop));

  const BOOL dark = readAppsUseLightTheme() ? FALSE : TRUE;
  DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                        sizeof(dark));
}

bool WindowsWindowStyle::usesBackdrop() const {
  return isWindows11OrLater(currentBuildNumber());
}

bool WindowsWindowStyle::prefersDarkTheme() const {
  return !readAppsUseLightTheme();
}
