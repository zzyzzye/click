# ClickFlow Windows 安装包与 Release 发布设计

日期：2026-07-30
状态：已确认

## 1. 目标

为 ClickFlow 建立第一版简洁、可重复的 Windows 发布链路：开发者从 GitHub 网页手动触发 Windows 打包工作流，由 GitHub 托管 Runner 完成 Release 构建、测试、Qt 运行库部署、安装器生成和 SHA-256 校验文件生成；随后下载工作流产物，在 GitHub 网页手动创建 Release 并上传。

安装器必须遵循常见 Windows 桌面软件行为：安装到 Program Files、请求管理员权限、出现在“设置 → 应用”和控制面板“程序和功能”中、支持覆盖升级，并提供完整卸载入口。

## 2. 已确认的产品决策

- 第一阶段仅支持 Windows x64。
- 使用 Inno Setup 生成 EXE 安装器。
- 为所有用户安装到 `C:\Program Files\ClickFlow`。
- 安装和卸载均请求管理员权限。
- 控制面板中的发布者显示为 `zzyzzye`。
- 开始菜单快捷方式是可选项，默认勾选。
- 桌面快捷方式是可选项，默认不勾选。
- 卸载时默认删除当前用户的全部 ClickFlow 宏和配置，不提供保留选项。
- 第一版不进行代码签名，但发布脚本为以后接入签名保留边界。
- 开发机受系统策略限制，不能运行安装包，因此不要求本机安装或运行 Inno Setup。
- 第一阶段增加仅支持网页手动触发的 GitHub Actions 工作流，不依赖 GitHub CLI，不自动创建标签或 Release。
- GitHub Release 仍由开发者在网页手动创建。

## 3. 方案选择

### 3.1 Inno Setup（采用）

Inno Setup 可以直接生成用户熟悉的 EXE 安装器，原生支持安装任务、快捷方式、卸载器、管理员权限、固定应用标识和覆盖升级。配置量较小，适合当前体量的 Qt 桌面应用。

### 3.2 WiX/MSI（暂不采用）

MSI 更适合企业批量部署、组策略、修复和集中资产管理，但项目结构、组件 GUID 和升级规则的维护成本更高。ClickFlow 当前没有企业部署需求。

### 3.3 MSIX（暂不采用）

MSIX 的安装隔离和卸载体验较现代，但签名和信任链是实际发布门槛，应用模型限制也会增加全局输入工具的验证成本。当前没有代码签名证书，因此不作为第一版方案。

## 4. 产物与目录结构

新增以下文件：

```text
installer/
  ClickFlow.iss
scripts/
  build-windows-release.ps1
  package-windows.ps1
dist/
  release/
    ClickFlow-<version>-win64-setup.exe
    ClickFlow-<version>-win64-setup.exe.sha256
.github/
  workflows/
    windows-package.yml
```

`dist/` 继续作为构建产物目录，不提交到 Git。

现有 `scripts/package-windows.ps1` 负责把可执行文件、Qt DLL、插件和 VC++ 运行库部署到临时 staging 目录。该脚本当前仍查找旧名称 `QtClicker.exe`，实现时必须改为当前产物名 `ClickFlow.exe`。

## 5. 版本来源

`CMakeLists.txt` 中的 `project(ClickFlow VERSION x.y.z)` 是唯一版本来源。

发布脚本读取该版本并传给 Inno Setup；安装器文件名、控制面板显示版本、EXE 版本资源和应用内版本均必须保持一致。脚本在无法读取合法三段式版本时应立即失败，不生成带错误版本的产物。

## 6. 打包脚本与云端工作流

`scripts/build-windows-release.ps1` 负责按固定顺序编排：

1. 校验 Windows、CMake、Qt、`windeployqt` 和 Inno Setup 编译器。
2. 从 `CMakeLists.txt` 读取版本号。
3. 配置并构建 Release。
4. 执行完整 CTest 测试集，任何失败立即停止。
5. 清理并重建本次 staging 目录。
6. 调用 `package-windows.ps1` 部署 `ClickFlow.exe` 及运行时依赖。
7. 调用 Inno Setup 编译安装器。
8. 计算安装器 SHA-256，并写入同名 `.sha256` 文件。
9. 输出最终产物绝对路径和后续手动发布提示。

脚本应允许显式传入构建目录、Qt bin 目录、Inno Setup 编译器路径和输出目录，同时为 GitHub Windows Runner 提供合理默认值。任何输入路径都要先规范化和验证。脚本仍可被具备相应工具的其他开发电脑复用，但受限开发机不运行它。

`.github/workflows/windows-package.yml` 只接受 `workflow_dispatch` 手动触发，并执行以下步骤：

1. 检出当前选择的分支或提交；
2. 准备固定版本的 Qt 6.8.3 MSVC x64；
3. 调用 `build-windows-release.ps1`；
4. 上传安装器和 SHA-256 为 GitHub Actions artifact；
5. 在工作流摘要中显示版本、产物名称和未签名提示。

工作流只授予读取仓库内容所需权限，不接触发布密钥，不自动创建标签、不推送 Git、不创建 GitHub Release，避免一次试运行意外产生公开发布状态。

## 7. 安装器行为

安装器使用固定且永久不变的 `AppId`，使后续版本能够识别已有安装并原位升级。不得因版本变化生成新的 `AppId`。

主要设置：

- 应用名称：`ClickFlow`
- 发布者：`zzyzzye`
- 架构：x64
- 默认目录：`{autopf}\ClickFlow`
- 权限：管理员安装模式
- 主程序：`ClickFlow.exe`
- 卸载显示图标：已安装的 `ClickFlow.exe`
- 禁用无实现意义的“修改”和“修复”入口
- 安装前检测并关闭正在运行的 ClickFlow，避免文件被占用
- 安装完成页提供可选的“运行 ClickFlow”

快捷方式通过 Inno Setup Tasks 实现：

- 开始菜单快捷方式：可选、默认勾选
- 桌面快捷方式：可选、默认不勾选

安装器应避免创建与用户选择不一致的额外入口。

## 8. 升级行为

相同 `AppId` 的新安装包覆盖旧版本安装目录。升级保留应用的用户数据，确保用户升级后宏和配置仍可使用；只有显式运行卸载器时才执行数据清理。

发布脚本不允许版本为空或与安装器输出名不一致。正式发布版本使用 `v<version>` Git 标签，例如 `v0.3.0`。

## 9. 干净卸载

卸载器自动移除：

- 安装目录中的程序文件和所有部署依赖；
- 安装器创建的开始菜单快捷方式；
- 安装器创建的桌面快捷方式；
- 控制面板和“设置 → 应用”中的卸载注册项；
- 当前 Windows 用户保存的 ClickFlow 宏目录；
- 当前 Windows 用户的 ClickFlow 配置注册表键；
- 安装器创建且已经为空的相关目录。

当前代码使用组织名 `OpenAI` 和内部应用名 `QtClicker` 存储设置与应用数据。第一版安装器必须按当前真实路径清理，不能只按展示名称 `ClickFlow` 猜测路径。实现时需要用自动化测试或运行时探针确认 Qt 在 Windows 上解析出的应用数据目录，并将清理路径限制到该应用专属目录和注册表键。

为遵守 Windows 用户数据边界，卸载器只清除执行卸载的 Windows 账户私有数据，不枚举、挂载或修改其他账户的用户配置文件。程序本体、公共快捷方式和系统级卸载入口仍会在全机范围完整删除。

清理规则不得使用过宽通配符，不得删除 `OpenAI`、`AppData` 或其他可能被多个应用共享的父目录。

## 10. 未签名安装包

首版安装包不签名。发布说明和 README 应明确：Windows SmartScreen 可能显示“未知发布者”，用户需要确认后继续安装。

脚本结构应把签名放在“安装器生成之后、校验文件生成之前”的独立阶段。第一版不接受、不读取也不保存证书密码或其他密钥；未来接入签名时使用系统证书存储或 CI 密钥管理。

## 11. GitHub Release 手动流程

云端打包成功后，开发者执行：

1. 在 GitHub Actions 页面手动运行 Windows 打包工作流；
2. 确认构建与完整测试通过，下载工作流 artifact；
3. 创建带说明的 `v<version>` 标签并推送；
4. 在 GitHub 仓库网页基于该标签创建 Release；
5. 上传 artifact 中的安装器与 `.sha256` 文件；
6. 填写本版本主要功能、修复、安装说明和未签名提示；
7. 发布 Release，并在允许运行安装包的干净 Windows 环境验证下载安装与卸载。

第一阶段不把标签和 Release 操作写进构建脚本或工作流。后续需要自动发布时，可在保持安装器和本地脚本不变的前提下扩展工作流。

## 12. 验证标准

发布功能完成必须满足：

- Release 配置构建成功；
- 完整 CTest 测试集通过；
- staging 目录包含可启动的 `ClickFlow.exe` 和所需 Qt/VC++ 依赖；
- 安装器能够在 Windows x64 上完成全用户安装；
- 控制面板和“设置 → 应用”能找到 ClickFlow 卸载入口；
- 两种快捷方式选项分别按用户选择生效；
- 安装后的应用能够启动；
- 同版本重装和更高版本覆盖安装不会产生重复卸载项；
- 卸载后程序目录、快捷方式、卸载项、当前用户宏和配置均被删除；
- `.sha256` 与安装器实际哈希一致；
- GitHub Actions 可以从网页手动触发，并上传只包含安装器和校验文件的 artifact；
- 受限开发机不需要安装或运行 Inno Setup；
- Git 工作区不包含 staging、安装器或其他生成产物。

## 13. 暂不包含

- macOS 安装包；
- ARM64 或 x86 安装包；
- 自动更新；
- GitHub Actions 自动触发、自动打标签或自动发布 Release；
- GitHub CLI 自动发布；
- 代码签名和时间戳服务；
- MSI、MSIX 或 Microsoft Store 发布；
- 跨 Windows 用户账户清理私有数据。
