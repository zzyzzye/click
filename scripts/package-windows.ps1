[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$BuildDir,

  [Parameter(Mandatory = $true)]
  [string]$OutputDir,

  [Parameter(Mandatory = $true)]
  [string]$QtBinDir,

  [ValidateSet("Debug", "Release")]
  [string]$Configuration = "Release",
  [ValidateSet("x64", "arm64")][string]$Architecture = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$modulePath = Join-Path $PSScriptRoot "WindowsReleaseTools.psm1"
Import-Module $modulePath -Force

$resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$resolvedQtBinDir = (Resolve-Path -LiteralPath $QtBinDir).Path
$windeployqtPath = Join-Path $resolvedQtBinDir "windeployqt.exe"

$sourceExe = Resolve-ClickFlowExecutable `
  -BuildDir $resolvedBuildDir -Configuration $Configuration

# 校验 PE 架构，避免错误工具链生成的安装包被错误标记。
$binary = [System.IO.File]::ReadAllBytes($sourceExe)
$peOffset = [BitConverter]::ToInt32($binary, 0x3c)
$machine = [BitConverter]::ToUInt16($binary, $peOffset + 4)
$expectedMachine = if ($Architecture -eq "arm64") { 0xaa64 } else { 0x8664 }
if ($machine -ne $expectedMachine) {
  throw "应用 EXE 架构与目标 $Architecture 不一致。"
}

if (-not (Test-Path -LiteralPath $windeployqtPath -PathType Leaf)) {
  throw "windeployqt.exe was not found in '$resolvedQtBinDir'."
}

$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null

$deployedExe = Join-Path $resolvedOutputDir "ClickFlow.exe"
Copy-Item -LiteralPath $sourceExe -Destination $deployedExe -Force

$deployMode = if ($Configuration -eq "Debug") { "--debug" } else { "--release" }
& $windeployqtPath $deployMode --compiler-runtime --no-translations `
  --dir $resolvedOutputDir $deployedExe

if ($LASTEXITCODE -ne 0) {
  throw "windeployqt failed with exit code $LASTEXITCODE."
}

if ($Configuration -eq "Release" -and
    -not (Test-Path -LiteralPath (Join-Path $resolvedOutputDir "vcruntime140.dll"))) {
  $vswhereCandidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe",
    "C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe"
  )
  $vswherePath = $vswhereCandidates |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
  if (-not $vswherePath) {
    throw "The Visual Studio Installer helper was not found; the VC runtime could not be staged."
  }

  $toolsComponent = if ($Architecture -eq "arm64") {
    "Microsoft.VisualStudio.Component.VC.Tools.ARM64"
  } else { "Microsoft.VisualStudio.Component.VC.Tools.x86.x64" }
  $visualStudioDir = & $vswherePath -latest -products * `
    -requires $toolsComponent `
    -property installationPath
  if ($LASTEXITCODE -ne 0 -or -not $visualStudioDir) {
    throw "未找到包含 $Architecture C++ 工具的 Visual Studio。"
  }

  $redistRoot = Join-Path $visualStudioDir "VC\Redist\MSVC"
  $runtimeDll = Get-ChildItem -LiteralPath $redistRoot -Filter "vcruntime140.dll" `
      -File -Recurse |
    Where-Object {
      $_.FullName -match "\\$Architecture\\Microsoft\.VC\d+\.CRT\\vcruntime140\.dll$" -and
      $_.FullName -notmatch "\\onecore\\"
    } |
    Sort-Object { $_.VersionInfo.FileVersionRaw } -Descending |
    Select-Object -First 1
  if (-not $runtimeDll) {
    throw "未找到 $Architecture VC++ 运行库目录。"
  }

  Get-ChildItem -LiteralPath $runtimeDll.DirectoryName -Filter "*.dll" -File |
    Copy-Item -Destination $resolvedOutputDir -Force
}

Write-Output "Packaged ClickFlow at $resolvedOutputDir"
