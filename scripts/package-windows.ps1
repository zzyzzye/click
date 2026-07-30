[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$BuildDir,

  [Parameter(Mandatory = $true)]
  [string]$OutputDir,

  [Parameter(Mandatory = $true)]
  [string]$QtBinDir,

  [ValidateSet("Debug", "Release")]
  [string]$Configuration = "Release"
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

  $visualStudioDir = & $vswherePath -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
  if ($LASTEXITCODE -ne 0 -or -not $visualStudioDir) {
    throw "A Visual Studio installation with the x64 C++ tools was not found."
  }

  $redistRoot = Join-Path $visualStudioDir "VC\Redist\MSVC"
  $runtimeDll = Get-ChildItem -LiteralPath $redistRoot -Filter "vcruntime140.dll" `
      -File -Recurse |
    Where-Object {
      $_.FullName -match "\\x64\\Microsoft\.VC\d+\.CRT\\vcruntime140\.dll$" -and
      $_.FullName -notmatch "\\onecore\\"
    } |
    Sort-Object { $_.VersionInfo.FileVersionRaw } -Descending |
    Select-Object -First 1
  if (-not $runtimeDll) {
    throw "The x64 VC runtime directory was not found below '$redistRoot'."
  }

  Get-ChildItem -LiteralPath $runtimeDll.DirectoryName -Filter "*.dll" -File |
    Copy-Item -Destination $resolvedOutputDir -Force
}

Write-Output "Packaged ClickFlow at $resolvedOutputDir"
