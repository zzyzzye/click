// 此程序不链接 Qt，独立等待原进程退出、提升安装器权限并以原用户重开软件。
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <string>
#include <cwchar>

namespace {
void message(const wchar_t* text) {
  MessageBoxW(nullptr, text, L"ClickFlow 更新", MB_OK | MB_ICONINFORMATION);
}
bool startApplication(const std::wstring& path) {
  std::wstring command = L"\"" + path + L"\"";
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                      &startup, &process))
    return false;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int count = 0;
  auto** args = CommandLineToArgvW(GetCommandLineW(), &count);
  if (!args || count != 5) {
    if (args)
      LocalFree(args);
    return 1;
  }
  const std::wstring installer = args[1], application = args[2], parameters = args[3];
  wchar_t* end = nullptr;
  const unsigned long parentPid = wcstoul(args[4], &end, 10);
  const bool validPid = parentPid != 0 && end && *end == 0;
  LocalFree(args);
  if (!validPid)
    return 1;
  HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, DWORD(parentPid));
  if (parent) {
    const DWORD wait = WaitForSingleObject(parent, 30000);
    CloseHandle(parent);
    if (wait != WAIT_OBJECT_0) {
      message(L"ClickFlow 未能退出，已取消更新。请关闭软件后重试。");
      return 2;
    }
  } else if (GetLastError() != ERROR_INVALID_PARAMETER) {
    message(L"无法确认 ClickFlow 已退出，已取消更新。请关闭软件后重试。");
    return 2;
  }
  // 助手始终保留原用户权限，只有安装器请求 UAC；安装后不由管理员进程启动主程序。
  SHELLEXECUTEINFOW info{};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
  info.lpVerb = L"runas";
  info.lpFile = installer.c_str();
  info.lpParameters = parameters.c_str();
  info.nShow = SW_SHOWNORMAL;
  bool installed = false;
  if (ShellExecuteExW(&info) && info.hProcess) {
    if (WaitForSingleObject(info.hProcess, INFINITE) == WAIT_OBJECT_0) {
      DWORD code = 1;
      installed = GetExitCodeProcess(info.hProcess, &code) && code == 0;
    }
    CloseHandle(info.hProcess);
    if (!installed)
      message(L"覆盖更新未完成，将重新打开当前可用版本。请稍后重试。");
  } else {
    message(GetLastError() == ERROR_CANCELLED ? L"已取消管理员授权，将重新打开原版本。"
                                              : L"无法启动安装器，将重新打开原版本。请稍后重试。");
  }
  if (!startApplication(application)) {
    message(L"无法重新打开 ClickFlow。请从开始菜单启动；若安装失败，请重新运行安装包。");
    return 3;
  }
  DeleteFileW(installer.c_str());
  // 助手自身的临时副本仍被占用，留待系统临时目录清理；不会留下第二套应用。
  return installed ? 0 : 4;
}
