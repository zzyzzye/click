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

$resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$resolvedQtBinDir = (Resolve-Path -LiteralPath $QtBinDir).Path
$windeployqtPath = Join-Path $resolvedQtBinDir "windeployqt.exe"

$configuredExe = Join-Path (Join-Path $resolvedBuildDir $Configuration) "QtClicker.exe"
$singleConfigExe = Join-Path $resolvedBuildDir "QtClicker.exe"
$sourceExe = if (Test-Path -LiteralPath $configuredExe -PathType Leaf) {
  $configuredExe
} elseif (Test-Path -LiteralPath $singleConfigExe -PathType Leaf) {
  $singleConfigExe
} else {
  throw "QtClicker.exe was not found for configuration '$Configuration' in '$resolvedBuildDir'."
}

if (-not (Test-Path -LiteralPath $windeployqtPath -PathType Leaf)) {
  throw "windeployqt.exe was not found in '$resolvedQtBinDir'."
}

$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null

$deployedExe = Join-Path $resolvedOutputDir "QtClicker.exe"
Copy-Item -LiteralPath $sourceExe -Destination $deployedExe -Force

$deployMode = if ($Configuration -eq "Debug") { "--debug" } else { "--release" }
& $windeployqtPath $deployMode --compiler-runtime --no-translations `
  --dir $resolvedOutputDir $deployedExe

if ($LASTEXITCODE -ne 0) {
  throw "windeployqt failed with exit code $LASTEXITCODE."
}

Write-Output "Packaged QtClicker at $resolvedOutputDir"
