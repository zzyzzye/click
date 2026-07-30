# ClickFlow Windows Installer and Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a repeatable local Windows x64 release command that tests ClickFlow, deploys its Qt runtime, creates a Windows-standard Inno Setup installer, and emits a SHA-256 file for manual GitHub Release upload.

**Architecture:** Keep packaging in three layers: a small testable PowerShell helper module for version/path rules, the existing staging script for `windeployqt`, and a release orchestrator that invokes CMake, CTest, staging, Inno Setup, and hashing in order. Keep installation policy declarative in one `.iss` file with a permanent `AppId`; Git and GitHub publishing remain deliberate manual steps.

**Tech Stack:** CMake 3.24+, C++20, Qt 6.8.3/MSVC x64, PowerShell, CTest, Inno Setup 6, GitHub Releases.

## Global Constraints

- First release supports Windows x64 only.
- Install for all users under `C:\Program Files\ClickFlow` and require administrator elevation.
- Control Panel publisher must be exactly `zzyzzye`.
- Start menu shortcut is optional and selected by default; desktop shortcut is optional and not selected by default.
- Uninstall always removes the uninstalling user's ClickFlow macros and settings; it must not enumerate or alter other user profiles.
- The permanent installer AppId is `{C2A5B39A-329F-4EF3-8E1F-9A2C2C93281C}` and must never change between releases.
- `project(ClickFlow VERSION x.y.z)` in `CMakeLists.txt` is the only release version source.
- The first release is unsigned and must document the possible Windows SmartScreen warning.
- Do not add GitHub Actions, GitHub CLI publishing, automatic tagging, automatic pushing, MSI, MSIX, x86, ARM64, or macOS packaging.
- Never read `.env` files or persist credentials, signing secrets, tokens, or passwords.

---

## File Structure

| File | Responsibility |
|---|---|
| `scripts/WindowsReleaseTools.psm1` | Pure/testable release helpers: project version parsing, artifact naming, safe path containment, and built executable resolution. |
| `tests/WindowsReleaseTools.Tests.ps1` | Dependency-free behavioral tests for version parsing, artifact naming, executable resolution, and safe path checks. |
| `CMakeLists.txt` | Registers the PowerShell release-contract test with CTest on Windows. |
| `scripts/package-windows.ps1` | Stages `ClickFlow.exe`, Qt plugins/DLLs, and the MSVC runtime into a supplied directory. |
| `installer/ClickFlow.iss` | Declares all-users install, permanent AppId, optional shortcuts, upgrade behavior, and current-user data cleanup. |
| `scripts/build-windows-release.ps1` | Orchestrates configure/build/test/stage/compile/hash and prints manual publishing instructions. |
| `README.md` | Documents prerequisites, one-command packaging, SmartScreen behavior, install/uninstall behavior, and manual GitHub Release steps. |

The PowerShell module is intentionally small. It must not contain process orchestration or mutate Git/GitHub state. The orchestrator owns commands; the Inno file owns Windows installation policy.

---

### Task 1: Add testable Windows release helpers

**Files:**
- Create: `scripts/WindowsReleaseTools.psm1`
- Create: `tests/WindowsReleaseTools.Tests.ps1`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `Get-ClickFlowProjectVersion([string] $CMakeListsPath) -> string`
- Produces: `Get-ClickFlowInstallerBaseName([string] $Version) -> string`
- Produces: `Resolve-ClickFlowExecutable([string] $BuildDir, [string] $Configuration) -> string`
- Produces: `Assert-ClickFlowPathWithin([string] $Path, [string] $ParentPath) -> void`, throwing when the path is not strictly below the parent.
- Consumes: only PowerShell built-ins; no Pester dependency.

- [ ] **Step 1: Create a failing dependency-free PowerShell test harness**

Create `tests/WindowsReleaseTools.Tests.ps1` with these assertions and cases:

```powershell
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$modulePath = Join-Path $repositoryRoot "scripts\WindowsReleaseTools.psm1"
Import-Module $modulePath -Force

function Assert-Equal($Expected, $Actual, [string]$Message) {
  if ($Expected -ne $Actual) {
    throw "$Message Expected '$Expected', got '$Actual'."
  }
}

function Assert-Throws([scriptblock]$Action, [string]$Message) {
  try {
    & $Action
  } catch {
    return
  }
  throw "$Message Expected an exception."
}

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
  ("ClickFlowReleaseTests-" + [guid]::NewGuid().ToString("N"))
try {
  New-Item -ItemType Directory -Path $testRoot -Force | Out-Null

  $validCMake = Join-Path $testRoot "CMakeLists.txt"
  Set-Content -LiteralPath $validCMake `
    -Value "project(ClickFlow VERSION 1.2.3 LANGUAGES CXX)" -Encoding UTF8
  Assert-Equal "1.2.3" (Get-ClickFlowProjectVersion $validCMake) `
    "Version parsing failed."

  $invalidCMake = Join-Path $testRoot "InvalidCMakeLists.txt"
  Set-Content -LiteralPath $invalidCMake `
    -Value "project(ClickFlow LANGUAGES CXX)" -Encoding UTF8
  Assert-Throws { Get-ClickFlowProjectVersion $invalidCMake } `
    "Invalid project versions must fail."

  Assert-Equal "ClickFlow-1.2.3-win64-setup" `
    (Get-ClickFlowInstallerBaseName "1.2.3") "Artifact naming failed."
  Assert-Throws { Get-ClickFlowInstallerBaseName "1.2" } `
    "Non-semantic versions must fail."

  $buildDir = Join-Path $testRoot "build"
  $releaseDir = Join-Path $buildDir "Release"
  New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
  $releaseExe = Join-Path $releaseDir "ClickFlow.exe"
  New-Item -ItemType File -Path $releaseExe | Out-Null
  Assert-Equal $releaseExe `
    (Resolve-ClickFlowExecutable $buildDir "Release") `
    "Multi-config executable resolution failed."

  Assert-ClickFlowPathWithin (Join-Path $testRoot "dist\release") $testRoot
  Assert-Throws { Assert-ClickFlowPathWithin $testRoot $testRoot } `
    "A parent path must not be accepted as its own child."
  Assert-Throws { Assert-ClickFlowPathWithin (Split-Path $testRoot -Parent) $testRoot } `
    "Paths outside the parent must fail."
} finally {
  $systemTemp = [System.IO.Path]::GetTempPath()
  Assert-ClickFlowPathWithin $testRoot $systemTemp
  Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Output "Windows release helper tests passed."
```

- [ ] **Step 2: Run the test directly and verify it fails**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\WindowsReleaseTools.Tests.ps1
```

Expected: FAIL because `scripts/WindowsReleaseTools.psm1` does not exist.

- [ ] **Step 3: Implement the minimal helper module**

Create `scripts/WindowsReleaseTools.psm1`:

```powershell
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ClickFlowProjectVersion {
  [CmdletBinding()]
  param([Parameter(Mandatory = $true)][string]$CMakeListsPath)

  $content = Get-Content -LiteralPath $CMakeListsPath -Raw
  $match = [regex]::Match(
    $content,
    'project\s*\(\s*ClickFlow\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)(?:\s|\))',
    [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
  )
  if (-not $match.Success) {
    throw "A ClickFlow x.y.z project version was not found in '$CMakeListsPath'."
  }
  return $match.Groups[1].Value
}

function Get-ClickFlowInstallerBaseName {
  [CmdletBinding()]
  param([Parameter(Mandatory = $true)][string]$Version)

  if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
    throw "Release version '$Version' must use x.y.z format."
  }
  return "ClickFlow-$Version-win64-setup"
}

function Resolve-ClickFlowExecutable {
  [CmdletBinding()]
  param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][ValidateSet("Debug", "Release")]
    [string]$Configuration
  )

  $resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
  $candidates = @(
    (Join-Path (Join-Path $resolvedBuildDir $Configuration) "ClickFlow.exe"),
    (Join-Path $resolvedBuildDir "ClickFlow.exe")
  )
  $result = $candidates |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
  if (-not $result) {
    throw "ClickFlow.exe was not found for '$Configuration' in '$resolvedBuildDir'."
  }
  return $result
}

function Assert-ClickFlowPathWithin {
  [CmdletBinding()]
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][string]$ParentPath
  )

  $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
  $fullParent = [System.IO.Path]::GetFullPath($ParentPath).TrimEnd('\', '/')
  $prefix = $fullParent + [System.IO.Path]::DirectorySeparatorChar
  if (-not $fullPath.StartsWith(
      $prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Path '$fullPath' must be inside '$fullParent'."
  }
}

Export-ModuleMember -Function `
  Get-ClickFlowProjectVersion, `
  Get-ClickFlowInstallerBaseName, `
  Resolve-ClickFlowExecutable, `
  Assert-ClickFlowPathWithin
```

- [ ] **Step 4: Run the direct PowerShell test and verify it passes**

Run the command from Step 2.

Expected: PASS and output `Windows release helper tests passed.`

- [ ] **Step 5: Register the helper test in CTest**

Inside the existing `if(WIN32)` section in `CMakeLists.txt`, add:

```cmake
find_program(CLICKFLOW_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
add_test(
  NAME WindowsReleaseTools
  COMMAND
    "${CLICKFLOW_POWERSHELL_EXECUTABLE}"
    -NoProfile
    -ExecutionPolicy Bypass
    -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/WindowsReleaseTools.Tests.ps1"
)
```

- [ ] **Step 6: Reconfigure and run the registered test**

Run:

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmake -S . -B build/windows-vs2026-debug `
  -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64
& $ctest --test-dir build/windows-vs2026-debug -C Debug `
  -R '^WindowsReleaseTools$' --output-on-failure
```

Expected: `WindowsReleaseTools` passes.

- [ ] **Step 7: Commit the helper boundary**

```powershell
git add CMakeLists.txt scripts/WindowsReleaseTools.psm1 `
  tests/WindowsReleaseTools.Tests.ps1
git commit -m "test: 增加Windows发布工具测试"
```

---

### Task 2: Correct and harden Qt runtime staging

**Files:**
- Modify: `scripts/package-windows.ps1`

**Interfaces:**
- Consumes: `Resolve-ClickFlowExecutable(BuildDir, Configuration)` from Task 1.
- Produces: a staging directory whose root executable is exactly `ClickFlow.exe`.
- Produces: a non-zero exit when the executable, Qt deployment tool, or VC++ runtime cannot be found.

- [ ] **Step 1: Run the existing packaging behavior and verify the artifact-name regression**

Build the real Release target, then invoke the real staging script:

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake --build build/windows-vs2026-debug --config Release --parallel
.\scripts\package-windows.ps1 `
  -BuildDir .\build\windows-vs2026-debug `
  -OutputDir .\dist\staging-red `
  -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin `
  -Configuration Release
```

Expected: the build succeeds, then packaging fails because it searches for obsolete `QtClicker.exe` even though the real output is `ClickFlow.exe`. This is the production behavior the fix must change.

- [ ] **Step 2: Update the staging script to consume the helper module**

At the top of `scripts/package-windows.ps1`, import:

```powershell
$modulePath = Join-Path $PSScriptRoot "WindowsReleaseTools.psm1"
Import-Module $modulePath -Force
```

Replace both legacy candidate calculations with:

```powershell
$sourceExe = Resolve-ClickFlowExecutable `
  -BuildDir $resolvedBuildDir -Configuration $Configuration
```

Change the destination executable and final message to `ClickFlow.exe`/`ClickFlow`:

```powershell
$deployedExe = Join-Path $resolvedOutputDir "ClickFlow.exe"
Copy-Item -LiteralPath $sourceExe -Destination $deployedExe -Force
```

Keep the current `windeployqt --compiler-runtime --no-translations` behavior and MSVC runtime fallback unchanged.

- [ ] **Step 3: Run the helper and CTest regression tests**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tests\WindowsReleaseTools.Tests.ps1
$ctest = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $ctest --test-dir build/windows-vs2026-debug -C Debug `
  -R '^WindowsReleaseTools$' --output-on-failure
```

Expected: both pass.

- [ ] **Step 4: Exercise real staging again**

Run:

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake --build build/windows-vs2026-debug --config Release --parallel
.\scripts\package-windows.ps1 `
  -BuildDir .\build\windows-vs2026-debug `
  -OutputDir .\dist\staging-smoke `
  -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin `
  -Configuration Release
Test-Path .\dist\staging-smoke\ClickFlow.exe
```

Expected: packaging succeeds and `Test-Path` prints `True`.

- [ ] **Step 5: Commit the staging fix**

```powershell
git add scripts/package-windows.ps1
git commit -m "fix: 修正Windows发布产物名称"
```

---

### Task 3: Add the Windows-standard Inno Setup installer

**Files:**
- Create: `installer/ClickFlow.iss`

**Interfaces:**
- Consumes Inno preprocessor definitions: `AppVersion`, `SourceDir`, and `OutputDir`.
- Produces: `ClickFlow-<version>-win64-setup.exe` in `OutputDir`.
- Uses permanent AppId: `{C2A5B39A-329F-4EF3-8E1F-9A2C2C93281C}`.
- Deletes current-user macro data at `{userappdata}\OpenAI\QtClicker` and settings at `HKCU\Software\OpenAI\QtClicker` during uninstall.

- [ ] **Step 1: Invoke the real installer compiler and verify the missing definition fails**

After Inno Setup 6 is available, run:

```powershell
$iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
& $iscc `
  '/DAppVersion=0.3.0' `
  '/DSourceDir=D:\ZW\click\dist\staging-smoke' `
  '/DOutputDir=D:\ZW\click\dist\release-smoke' `
  '.\installer\ClickFlow.iss'
```

Expected: FAIL because `installer/ClickFlow.iss` does not exist. The later green check uses the same compiler and inputs, so it validates the actual configuration rather than its source text.

- [ ] **Step 2: Create the installer definition**

Create `installer/ClickFlow.iss` with these sections and exact policies:

```ini
#ifndef AppVersion
  #error AppVersion must be provided by the release script.
#endif
#ifndef SourceDir
  #error SourceDir must be provided by the release script.
#endif
#ifndef OutputDir
  #error OutputDir must be provided by the release script.
#endif

#define AppName "ClickFlow"
#define AppPublisher "zzyzzye"
#define AppExeName "ClickFlow.exe"
#define AppIdValue "C2A5B39A-329F-4EF3-8E1F-9A2C2C93281C"

[Setup]
AppId={{{#AppIdValue}}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-{#AppVersion}-win64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
CloseApplications=force
RestartApplications=no
ChangesAssociations=no
DisableWelcomePage=no

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "startmenuicon"; Description: "创建开始菜单快捷方式"; GroupDescription: "快捷方式："; Flags: checkedonce
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式："; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: startmenuicon
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "运行 {#AppName}"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\OpenAI\QtClicker"; Flags: uninsdeletekey

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\OpenAI\QtClicker"
```

Do not add a broad delete rule for `{userappdata}\OpenAI` or any other parent directory.

- [ ] **Step 3: Compile the installer with the real Inno Setup compiler**

After installing Inno Setup 6, run:

```powershell
$iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
& $iscc `
  '/DAppVersion=0.3.0' `
  '/DSourceDir=D:\ZW\click\dist\staging-smoke' `
  '/DOutputDir=D:\ZW\click\dist\release-smoke' `
  '.\installer\ClickFlow.iss'
```

Expected: exit code 0 and `dist/release-smoke/ClickFlow-0.3.0-win64-setup.exe` exists.

- [ ] **Step 4: Commit the installer definition**

```powershell
git add installer/ClickFlow.iss
git commit -m "feat: 增加Windows安装器配置"
```

---

### Task 4: Add the one-command local release orchestrator

**Files:**
- Create: `scripts/build-windows-release.ps1`

**Interfaces:**
- Parameters: `BuildDir`, `OutputDir`, `QtBinDir`, `CMakePath`, `InnoCompilerPath`.
- Consumes: all four exported helper functions from Task 1.
- Consumes: `scripts/package-windows.ps1` and `installer/ClickFlow.iss`.
- Produces: installer EXE and `<installer>.sha256`; no Git tags, pushes, or GitHub mutations.

- [ ] **Step 1: Invoke the intended release entry point and verify it is missing**

Run:

```powershell
.\scripts\build-windows-release.ps1 `
  -BuildDir .\build\windows-release `
  -OutputDir .\dist\release `
  -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin
```

Expected: FAIL because the release script does not exist. The green acceptance check will execute this same entry point against the real CMake, CTest, Qt deployment, and Inno compiler.

- [ ] **Step 2: Implement path and tool validation in the orchestrator**

Create `scripts/build-windows-release.ps1` with:

```powershell
[CmdletBinding()]
param(
  [string]$BuildDir = "",
  [string]$OutputDir = "",
  [string]$QtBinDir = "",
  [string]$CMakePath = "",
  [string]$InnoCompilerPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot "WindowsReleaseTools.psm1") -Force

if (-not $BuildDir) {
  $BuildDir = Join-Path $repositoryRoot "build\windows-release"
}
if (-not $OutputDir) {
  $OutputDir = Join-Path $repositoryRoot "dist\release"
}

$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
Assert-ClickFlowPathWithin $BuildDir $repositoryRoot
Assert-ClickFlowPathWithin $OutputDir $repositoryRoot

function Resolve-ToolPath {
  param(
    [string]$ExplicitPath,
    [string]$CommandName,
    [string[]]$FallbackPaths
  )
  if ($ExplicitPath) {
    return (Resolve-Path -LiteralPath $ExplicitPath).Path
  }
  $command = Get-Command $CommandName -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Source
  }
  $fallback = $FallbackPaths |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
  if ($fallback) {
    return (Resolve-Path -LiteralPath $fallback).Path
  }
  throw "$CommandName was not found; pass its explicit path."
}
```

Resolve CMake using `cmake.exe` plus the Visual Studio 18 and Visual Studio 17 bundled paths. Resolve Inno Setup using `ISCC.exe` plus both standard 64-bit and 32-bit Program Files locations. Resolve Qt from `-QtBinDir`; if it is absent, use the directory containing `windeployqt.exe` from `PATH`, otherwise fail with a message showing `-QtBinDir`.

Use these exact resolutions immediately after `Resolve-ToolPath`:

```powershell
$cmakePath = Resolve-ToolPath `
  -ExplicitPath $CMakePath `
  -CommandName "cmake.exe" `
  -FallbackPaths @(
    "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )

$innoPath = Resolve-ToolPath `
  -ExplicitPath $InnoCompilerPath `
  -CommandName "ISCC.exe" `
  -FallbackPaths @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
  )

if ($QtBinDir) {
  $QtBinDir = (Resolve-Path -LiteralPath $QtBinDir).Path
} else {
  $winDeployQt = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
  if (-not $winDeployQt) {
    throw "windeployqt.exe was not found; pass -QtBinDir."
  }
  $QtBinDir = Split-Path -Parent $winDeployQt.Source
}
if (-not (Test-Path -LiteralPath `
    (Join-Path $QtBinDir "windeployqt.exe") -PathType Leaf)) {
  throw "QtBinDir '$QtBinDir' does not contain windeployqt.exe."
}
```

- [ ] **Step 3: Implement ordered build, test, staging, installer, and hashing commands**

Continue the script with this sequence:

```powershell
$cmakeLists = Join-Path $repositoryRoot "CMakeLists.txt"
$version = Get-ClickFlowProjectVersion $cmakeLists
$artifactBaseName = Get-ClickFlowInstallerBaseName $version
$qtBin = (Resolve-Path -LiteralPath $QtBinDir).Path
$qtRoot = Split-Path -Parent $qtBin
$ctestPath = Join-Path (Split-Path -Parent $cmakePath) "ctest.exe"
if (-not (Test-Path -LiteralPath $ctestPath -PathType Leaf)) {
  throw "ctest.exe was not found beside '$cmakePath'."
}

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
& $cmakePath -S $repositoryRoot -B $BuildDir -A x64 `
  "-DCMAKE_PREFIX_PATH=$qtRoot"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

& $cmakePath --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Release build failed." }

& $ctestPath --test-dir $BuildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Release tests failed." }

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$stagingRoot = Join-Path $OutputDir ".staging"
Assert-ClickFlowPathWithin $stagingRoot $OutputDir
if (Test-Path -LiteralPath $stagingRoot) {
  Remove-Item -LiteralPath $stagingRoot -Recurse -Force
}
$stagingApp = Join-Path $stagingRoot "ClickFlow"

& (Join-Path $PSScriptRoot "package-windows.ps1") `
  -BuildDir $BuildDir -OutputDir $stagingApp `
  -QtBinDir $qtBin -Configuration Release
if ($LASTEXITCODE -ne 0) { throw "Runtime staging failed." }

& $innoPath `
  "/DAppVersion=$version" `
  "/DSourceDir=$stagingApp" `
  "/DOutputDir=$OutputDir" `
  (Join-Path $repositoryRoot "installer\ClickFlow.iss")
if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed." }

$installerPath = Join-Path $OutputDir ($artifactBaseName + ".exe")
if (-not (Test-Path -LiteralPath $installerPath -PathType Leaf)) {
  throw "Expected installer '$installerPath' was not generated."
}

# Future signing belongs here, before hashing. The first release is unsigned.
$hash = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLowerInvariant()
$hashPath = $installerPath + ".sha256"
Set-Content -LiteralPath $hashPath `
  -Value "$hash  $([System.IO.Path]::GetFileName($installerPath))" `
  -Encoding ASCII

Remove-Item -LiteralPath $stagingRoot -Recurse -Force
Write-Output "Windows release package created:"
Write-Output "  $installerPath"
Write-Output "  $hashPath"
Write-Warning "This installer is unsigned and may trigger Windows SmartScreen."
Write-Output "Create tag v$version, push it, then upload both files to GitHub Releases."
```

Use resolved variables `$cmakePath` and `$innoPath` from Step 3. Do not add calls to `git`, `gh`, or a network API.

- [ ] **Step 4: Parse the release script before executing external tools**

Run:

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

Expected: no parser errors.

- [ ] **Step 5: Run the full local release command**

Run:

```powershell
.\scripts\build-windows-release.ps1 `
  -BuildDir .\build\windows-release `
  -OutputDir .\dist\release `
  -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin `
  -CMakePath 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  -InnoCompilerPath 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
```

Expected:

- Release configuration builds.
- Full CTest suite passes.
- `dist/release/ClickFlow-0.3.0-win64-setup.exe` exists.
- Its `.sha256` file exists and contains the matching lowercase SHA-256.
- `dist/release/.staging` no longer exists after success.

- [ ] **Step 6: Commit the local release orchestrator**

```powershell
git add scripts/build-windows-release.ps1
git commit -m "build: 增加Windows本地发布脚本"
```

---

### Task 5: Document installation and manual GitHub Release publishing

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: the final command and artifact names from Task 4.
- Produces: exact operator instructions; no new runtime interface.

- [ ] **Step 1: Replace the obsolete Windows packaging example**

Replace the current `### Windows 打包` section with prerequisites and the one-command flow:

```markdown
### Windows 安装包

前置依赖：

- Visual Studio 的 x64 C++ 工具链与 CMake
- Qt 6.8.3 MSVC x64
- Inno Setup 6

生成 Release 安装包：

```powershell
.\scripts\build-windows-release.ps1 `
  -QtBinDir D:\Qt\6.8.3\msvc2022_64\bin
```

脚本会依次构建 Release、执行完整测试、部署 Qt/VC++ 运行库、生成安装器和 SHA-256 校验文件。产物位于 `dist/release/`。
```

Document the optional path parameters when tools are not discoverable automatically.

- [ ] **Step 2: Document installer and uninstall behavior**

Add concise bullets stating:

- Installation is for all users under `Program Files` and requests UAC.
- Start menu and desktop shortcuts are selected independently in the wizard.
- ClickFlow appears in Settings and Control Panel uninstall lists.
- Uninstall removes program files, shortcuts, the uninstall entry, and the uninstalling user's macros/settings.
- The unsigned first release may show a SmartScreen “unknown publisher” warning.

- [ ] **Step 3: Document the manual GitHub Release checklist**

Add exact commands, leaving version replacement explicit:

```powershell
git status --short
git push origin main
git tag -a v0.3.0 -m "release: 发布 v0.3.0"
git push origin v0.3.0
```

Then instruct the operator to create a GitHub Release from `v0.3.0`, upload the installer and `.sha256`, include the SmartScreen note, and test the download on a clean Windows environment. State clearly that the build script never pushes or publishes automatically.

- [ ] **Step 4: Verify documentation and repository hygiene**

Run:

```powershell
rg -n "QtClicker\.exe|package-windows\.ps1" README.md
git status --short
git diff --check
```

Expected: README no longer tells users to package the obsolete executable directly; only intended source/docs files are modified; `dist/` remains ignored.

- [ ] **Step 5: Commit documentation**

```powershell
git add README.md
git commit -m "docs: 补充Windows安装与发布说明"
```

---

### Task 6: Verify install, upgrade, shortcuts, and clean uninstall end to end

**Files:**
- Modify only if verification reveals a defect: `installer/ClickFlow.iss`, `scripts/build-windows-release.ps1`, `scripts/package-windows.ps1`, or their corresponding tests.

**Interfaces:**
- Consumes: final installer and checksum from Task 4.
- Produces: verification evidence; no new product interface.

- [ ] **Step 1: Run the complete Debug and Release test suites from fresh output**

Run:

```powershell
$ctest = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
& $ctest --test-dir build/windows-release -C Release --output-on-failure
```

Expected: every registered test passes in both configurations.

- [ ] **Step 2: Verify the SHA-256 sidecar**

Run:

```powershell
$installer = Resolve-Path .\dist\release\ClickFlow-0.3.0-win64-setup.exe
$actual = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
$recorded = (Get-Content -LiteralPath ($installer.Path + '.sha256')).Split(' ')[0]
if ($actual -ne $recorded) { throw 'SHA-256 mismatch.' }
```

Expected: no exception.

- [ ] **Step 3: Test default installation and Control Panel registration**

Run the installer interactively, keep the default start menu option, leave desktop shortcut unselected, and finish installation. Verify:

- UAC elevation occurs.
- `C:\Program Files\ClickFlow\ClickFlow.exe` exists and launches.
- Start menu contains ClickFlow.
- Desktop does not contain ClickFlow.
- ClickFlow appears in both Settings Apps and Control Panel Programs and Features.
- Display version is `0.3.0` and publisher is `zzyzzye`.

- [ ] **Step 4: Test the alternate shortcut selection and same-version upgrade**

Run the same installer again. Select the desktop shortcut and clear the start menu shortcut. Verify there remains exactly one ClickFlow uninstall entry, the application still launches, and the shortcut choices match the second installation.

- [ ] **Step 5: Seed real user data and verify clean uninstall**

Launch ClickFlow, save one profile and one macro, then confirm these application-owned locations exist:

```powershell
$roamingData = [Environment]::GetFolderPath(
  [Environment+SpecialFolder]::ApplicationData
)
Test-Path (Join-Path $roamingData 'OpenAI\QtClicker\macros')
Test-Path 'HKCU:\Software\OpenAI\QtClicker'
```

Use Control Panel to uninstall ClickFlow. Verify:

```powershell
Test-Path 'C:\Program Files\ClickFlow'
Test-Path (Join-Path $roamingData 'OpenAI\QtClicker')
Test-Path 'HKCU:\Software\OpenAI\QtClicker'
```

Expected: all three commands print `False`; both optional shortcuts and the uninstall entry are absent.

- [ ] **Step 6: Check final source tree and commits**

Run:

```powershell
git diff --check
git status --short --branch
git log --oneline -6
```

Expected: source worktree is clean, generated files remain ignored, and the feature is represented by small Conventional Commit-style Chinese commits.

- [ ] **Step 7: Commit only if verification required a fix**

If a defect was fixed, rerun the failing verification plus both full CTest suites, then commit only the affected code and test:

```powershell
git add installer scripts tests CMakeLists.txt README.md
git commit -m "fix: 修正Windows安装卸载流程"
```

If no defect was found, do not create an empty verification commit.

---

## Manual Release Handoff

After Task 6 passes and the user explicitly authorizes external publication:

1. Push `main`.
2. Create and push annotated tag `v0.3.0`.
3. Create the GitHub Release from that tag in the browser.
4. Upload `ClickFlow-0.3.0-win64-setup.exe` and its `.sha256` file.
5. Publish release notes including the unsigned SmartScreen warning.

Do not perform these external mutations merely because a local package succeeded.
