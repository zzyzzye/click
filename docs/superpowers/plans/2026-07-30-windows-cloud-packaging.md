# ClickFlow Windows Cloud Packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate the ClickFlow Windows x64 installer and SHA-256 entirely on a manually triggered GitHub Actions Windows Runner, so the restricted development computer never installs or runs an installer package.

**Architecture:** Keep Inno Setup policy in `installer/ClickFlow.iss` and orchestration in `scripts/build-windows-release.ps1`; both remain reusable outside CI. A `workflow_dispatch` workflow installs Qt, calls the same script on GitHub's Windows 2025 image, and uploads only the versioned installer and checksum as an Actions artifact. Tagging and GitHub Release creation stay manual.

**Tech Stack:** GitHub Actions `windows-2025`, CMake, CTest, Qt 6.8.3 MSVC 2022 x64, PowerShell, Inno Setup 6, actions/upload-artifact.

## Global Constraints

- The development computer must not install or run Inno Setup or the generated ClickFlow installer.
- The workflow trigger is only `workflow_dispatch`; do not publish on push or tag.
- Workflow permissions are `contents: read` only.
- Pin third-party actions to full commit SHAs and annotate their release versions.
- Use Qt `6.8.3`, target `desktop`, architecture `win64_msvc2022_64`.
- Produce `ClickFlow-<version>-win64-setup.exe` and its `.sha256` sidecar.
- Preserve installer AppId `{C2A5B39A-329F-4EF3-8E1F-9A2C2C93281C}`.
- Do not create tags, push commits, or create GitHub Releases from the workflow.
- The first installer remains unsigned and may trigger SmartScreen.
- Never read `.env` files or store credentials, signing secrets, tokens, or passwords.

---

### Task 1: Add the Inno Setup installer definition

**Files:**
- Create: `installer/ClickFlow.iss`

**Interfaces:**
- Consumes preprocessor definitions `AppVersion`, `SourceDir`, and `OutputDir`.
- Produces `ClickFlow-<version>-win64-setup.exe`.
- Installs for all users and removes current-user data during explicit uninstall.

- [ ] **Step 1: Create `installer/ClickFlow.iss`**

Use the exact setup, task, file, icon, run, registry, and uninstall rules approved in `docs/superpowers/specs/2026-07-30-windows-installer-release-design.md`. The fixed declarations are:

```ini
#define AppName "ClickFlow"
#define AppPublisher "zzyzzye"
#define AppExeName "ClickFlow.exe"
#define AppIdValue "C2A5B39A-329F-4EF3-8E1F-9A2C2C93281C"
```

The installer must use `PrivilegesRequired=admin`, `ArchitecturesAllowed=x64compatible`, `{autopf}\ClickFlow`, optional start-menu/desktop tasks, `uninsdeletekey` for `HKCU\Software\OpenAI\QtClicker`, and `{userappdata}\OpenAI\QtClicker` in `[UninstallDelete]`.

- [ ] **Step 2: Check source formatting and commit**

```powershell
git diff --check
git add installer/ClickFlow.iss
git commit -m "feat: 增加Windows安装器配置"
```

Compilation is intentionally deferred to Task 3 because the development computer cannot run Inno Setup. The first real compiler run is the cloud acceptance test.

---

### Task 2: Add the reusable release orchestration script

**Files:**
- Create: `scripts/build-windows-release.ps1`

**Interfaces:**
- Parameters: `BuildDir`, `OutputDir`, `QtBinDir`, `CMakePath`, `InnoCompilerPath`.
- Consumes helper functions from `scripts/WindowsReleaseTools.psm1`.
- Consumes `scripts/package-windows.ps1` and `installer/ClickFlow.iss`.
- Produces the versioned installer and checksum without mutating Git or GitHub.

- [ ] **Step 1: Verify the intended entry point is absent**

```powershell
.\scripts\build-windows-release.ps1 -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin
```

Expected: command-not-found failure because the script does not exist.

- [ ] **Step 2: Implement preflight and safe-path validation**

Import `WindowsReleaseTools.psm1`; resolve defaults below the repository (`build/windows-release`, `dist/release`); reject paths outside the repository with `Assert-ClickFlowPathWithin`. Resolve CMake from an explicit path, `PATH`, Visual Studio 18, or Visual Studio 2022. Resolve ISCC from an explicit path, `PATH`, or the two standard Inno Setup 6 locations. Resolve Qt from `-QtBinDir` or `windeployqt.exe` on `PATH`.

- [ ] **Step 3: Implement the ordered release pipeline**

Run configure, Release build, full CTest, staging, ISCC, and SHA-256 in that order. Check `$LASTEXITCODE` after every external command. Verify the expected EXE exists before hashing. Delete only the verified `dist/release/.staging` child after success. Print final paths and an unsigned-installer warning. Do not call `git`, `gh`, or network APIs.

- [ ] **Step 4: Parse the PowerShell script locally**

```powershell
$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
  (Resolve-Path '.\scripts\build-windows-release.ps1'),
  [ref]$tokens,
  [ref]$errors
) | Out-Null
if ($errors.Count -ne 0) { throw $errors[0].Message }
```

Expected: no parser errors. Do not execute the release pipeline locally.

- [ ] **Step 5: Commit**

```powershell
git add scripts/build-windows-release.ps1
git commit -m "build: 增加Windows发布编排脚本"
```

---

### Task 3: Add the manually triggered GitHub Actions workflow

**Files:**
- Create: `.github/workflows/windows-package.yml`

**Interfaces:**
- Trigger: GitHub web UI `workflow_dispatch` only.
- Runner: `windows-2025`, timeout 45 minutes.
- Produces Actions artifact `ClickFlow-Windows-x64` retained for 14 days.

- [ ] **Step 1: Create the workflow with least privilege**

Use this dependency set:

```yaml
permissions:
  contents: read

steps:
  - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
  - uses: jurplel/install-qt-action@48d3ad6db93f3627c8ee7a0454bc6f3744f7e730 # v4.3.1
    with:
      version: '6.8.3'
      host: windows
      target: desktop
      arch: win64_msvc2022_64
      cache: true
  - uses: actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a # v7.0.1
```

The PowerShell build step calls:

```powershell
.\scripts\build-windows-release.ps1 `
  -QtBinDir (Join-Path $env:QT_ROOT_DIR 'bin')
```

Upload only `dist/release/*.exe` and `dist/release/*.sha256`, with `if-no-files-found: error`, `compression-level: 0`, and `retention-days: 14`. Append artifact names and the unsigned warning to `$GITHUB_STEP_SUMMARY`.

- [ ] **Step 2: Review workflow policy and commit**

Confirm the YAML has no `push`, `pull_request`, `release`, write permission, secret reference, or publishing command.

```powershell
git diff --check
git add .github/workflows/windows-package.yml
git commit -m "ci: 增加Windows云端打包流程"
```

- [ ] **Step 3: Push source and run the real cloud acceptance test**

After explicit push authorization, push `main`. In GitHub: Actions → Windows 安装包 → Run workflow → select `main`. Expected: configure/build/full CTest/staging/Inno/hash all pass and artifact `ClickFlow-Windows-x64` is downloadable.

If the run fails, capture the exact failing step, write a regression test where feasible, fix only that defect, commit, push, and rerun.

---

### Task 4: Document the operator flow and verify the artifact

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes the workflow from Task 3.
- Produces exact instructions for cloud packaging and manual Release publication.

- [ ] **Step 1: Replace local Inno prerequisites with cloud instructions**

Document: push the desired commit, open Actions, select “Windows 安装包”, run `main`, wait for green, and download `ClickFlow-Windows-x64`. State that the downloaded artifact is a ZIP containing the installer and checksum; the restricted development computer does not run either file.

- [ ] **Step 2: Document manual Release publication**

Document version/tag consistency, annotated tag commands, GitHub Release creation, upload of the EXE and `.sha256`, and the unsigned SmartScreen warning. State that the workflow never tags or publishes automatically.

- [ ] **Step 3: Commit documentation**

```powershell
git diff --check
git add README.md
git commit -m "docs: 补充Windows云端打包说明"
```

- [ ] **Step 4: Verify on an allowed Windows computer**

On another Windows x64 computer that permits installers: verify UAC, Program Files installation, publisher/version in Settings and Control Panel, both shortcut choices, same-version reinstall without duplicate entries, application launch, and clean uninstall of program files, shortcuts, uninstall entry, macros, and settings.

Do not run this verification on the restricted development computer.
