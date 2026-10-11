# Windows 应用内覆盖更新

流程：检查正式 Release → 下载并展示进度 → SHA-256 校验 → 用户点击重启并更新 → 处理未保存内容 → 更新助手接管 → 覆盖原目录 → 重开 ClickFlow。

## 实现边界

- UpdateController 复用 GitHub 版本检查，匹配当前可执行程序的 x64/ARM64 安装包；明确区分检查、下载、就绪和安装交接，禁止重复启动。失败后可以重试。
- 下载按块写入 QSaveFile 并计算摘要，不将整包留在内存；网络超时 30 秒，安装包上限 512 MiB。下载有真实百分比和大小，未知总量、获取校验信息与复查文件不显示虚假进度。
- 校验后不自动退出。用户点击重启时异步复查磁盘文件，交接前再检查自动化是否停止，并复用未保存录制/流程的保存、放弃、取消。取消保留更新包；不重复弹出关闭确认。
- 覆盖更新只允许从本机注册安装目录启动的应用使用，安装器再次核对原 AppId 的 InstallLocation，拒绝另一个目录。固定 AppId、原目录与快捷方式选择保留，升级不运行卸载器或清理用户数据。
- 原生 WindowsUpdateHelper 不链接 Qt，静态链接 VC 运行库，临时副本不占用待覆盖文件。助手等待主程序退出，单独提升安装器权限并等待安装结束，再以助手原有用户权限重开应用。取消 UAC 时重开原版本；安装失败提示并尝试启动当前可用版本，不宣称自动回滚。
- 使用 Inno Setup 的 `/SILENT` 显示安装进度，跳过安装向导；`/NORESTART` 不重启电脑，`/NOCLOSEAPPLICATIONS` 与 `/NOFORCECLOSEAPPLICATIONS` 不强杀其他运行实例。更新模式不由提升权限的安装器重开软件。
- 助手移除已经使用的下载包；运行中的助手临时副本留在系统临时目录供系统清理，不留下第二套应用或卸载项。

## 本地验证与待验证事项

模拟网络与安装器的测试覆盖下载进度、重复点击、取消后迟到响应、重试、校验失败、网络/格式错误、任务运行中拒绝重启、未保存取消、助手启动失败、安装包变化及便携副本拒绝覆盖。页面离屏测试覆盖确定/不确定进度、键盘取消和重启，以及按钮位置稳定。

本机 macOS 只构建共用 UI 和更新控制器；未运行 Windows 安装器或真实覆盖更新。Windows x64/ARM64 原生构建、Inno Setup 编译、UAC 取消/其他管理员授权、原目录覆盖、快捷方式/卸载项唯一性、用户数据保留与自动重开，需目标系统验证。首次安装仍使用正常安装包；已安装旧客户端要先升级到包含本机制的版本。

2026-10-11：macOS Debug 构建通过，完整 11 个 CTest 套件通过；更新器测试使用模拟网络及假安装器，不接触真实 Release 下载或系统安装。下载/就绪截图、键盘操作与布局稳定性检查通过。`git diff --check` 和更新源文件 clang-format 检查通过。Frontend Design Premium 严格审查无发现，DESIGN.md lint 为 0 错误、8 个既有令牌警告。

依据：[Inno Setup 命令行](https://jrsoftware.org/ishelp/topic_setupcmdline.htm)、[沿用原安装目录](https://jrsoftware.org/ishelp/topic_setup_usepreviousappdir.htm)、[安装后程序运行权限](https://jrsoftware.org/ishelp/topic_runsection.htm)。
