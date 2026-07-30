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

$repositoryRoot = [System.IO.Path]::GetFullPath(
  (Split-Path -Parent $PSScriptRoot)
)
Import-Module (Join-Path $PSScriptRoot "WindowsReleaseTools.psm1") -Force

function Resolve-ToolPath {
  [CmdletBinding()]
  param(
    [string]$ExplicitPath,
    [Parameter(Mandatory = $true)][string]$CommandName,
    [string[]]$FallbackPaths = @()
  )

  if ($ExplicitPath) {
    $resolved = (Resolve-Path -LiteralPath $ExplicitPath).Path
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
      throw "Tool '$resolved' is not a file."
    }
    return $resolved
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

function Invoke-CheckedCommand {
  [CmdletBinding()]
  param(
    [Parameter(Mandatory = $true)][string]$Description,
    [Parameter(Mandatory = $true)][scriptblock]$Command
  )

  Write-Output "==> $Description"
  & $Command
  if ($LASTEXITCODE -ne 0) {
    throw "$Description failed with exit code $LASTEXITCODE."
  }
}

if (-not $BuildDir) {
  $BuildDir = Join-Path $repositoryRoot "build\windows-release"
}
if (-not $OutputDir) {
  $OutputDir = Join-Path $repositoryRoot "dist\release"
}

$resolvedBuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir)
Assert-ClickFlowPathWithin $resolvedBuildDir $repositoryRoot
Assert-ClickFlowPathWithin $resolvedOutputDir $repositoryRoot

$resolvedCMakePath = Resolve-ToolPath `
  -ExplicitPath $CMakePath `
  -CommandName "cmake.exe" `
  -FallbackPaths @(
    "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )

$resolvedInnoPath = Resolve-ToolPath `
  -ExplicitPath $InnoCompilerPath `
  -CommandName "ISCC.exe" `
  -FallbackPaths @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
  )

if ($QtBinDir) {
  $resolvedQtBinDir = (Resolve-Path -LiteralPath $QtBinDir).Path
} else {
  $winDeployQt = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
  if (-not $winDeployQt) {
    throw "windeployqt.exe was not found; pass -QtBinDir."
  }
  $resolvedQtBinDir = Split-Path -Parent $winDeployQt.Source
}

$winDeployQtPath = Join-Path $resolvedQtBinDir "windeployqt.exe"
if (-not (Test-Path -LiteralPath $winDeployQtPath -PathType Leaf)) {
  throw "QtBinDir '$resolvedQtBinDir' does not contain windeployqt.exe."
}

$qtRoot = Split-Path -Parent $resolvedQtBinDir
$ctestPath = Join-Path (Split-Path -Parent $resolvedCMakePath) "ctest.exe"
if (-not (Test-Path -LiteralPath $ctestPath -PathType Leaf)) {
  throw "ctest.exe was not found beside '$resolvedCMakePath'."
}

$version = Get-ClickFlowProjectVersion `
  (Join-Path $repositoryRoot "CMakeLists.txt")
$artifactBaseName = Get-ClickFlowInstallerBaseName $version
$installerPath = Join-Path $resolvedOutputDir ($artifactBaseName + ".exe")
$hashPath = $installerPath + ".sha256"

New-Item -ItemType Directory -Path $resolvedBuildDir -Force | Out-Null
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null

Invoke-CheckedCommand "Configure CMake" {
  & $resolvedCMakePath -S $repositoryRoot -B $resolvedBuildDir -A x64 `
    "-DCMAKE_PREFIX_PATH=$qtRoot"
}
Invoke-CheckedCommand "Build ClickFlow Release" {
  & $resolvedCMakePath --build $resolvedBuildDir --config Release --parallel
}
Invoke-CheckedCommand "Run the complete Release test suite" {
  & $ctestPath --test-dir $resolvedBuildDir -C Release --output-on-failure
}

$stagingRoot = Join-Path $resolvedOutputDir ".staging"
Assert-ClickFlowPathWithin $stagingRoot $resolvedOutputDir
if (Test-Path -LiteralPath $stagingRoot) {
  Remove-Item -LiteralPath $stagingRoot -Recurse -Force
}
$stagingApp = Join-Path $stagingRoot "ClickFlow"

Write-Output "==> Stage the Windows runtime"
& (Join-Path $PSScriptRoot "package-windows.ps1") `
  -BuildDir $resolvedBuildDir `
  -OutputDir $stagingApp `
  -QtBinDir $resolvedQtBinDir `
  -Configuration Release
if ($LASTEXITCODE -ne 0) {
  throw "Runtime staging failed with exit code $LASTEXITCODE."
}

Remove-Item -LiteralPath $installerPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $hashPath -Force -ErrorAction SilentlyContinue

Invoke-CheckedCommand "Compile the Inno Setup installer" {
  & $resolvedInnoPath `
    "/DAppVersion=$version" `
    "/DSourceDir=$stagingApp" `
    "/DOutputDir=$resolvedOutputDir" `
    (Join-Path $repositoryRoot "installer\ClickFlow.iss")
}

if (-not (Test-Path -LiteralPath $installerPath -PathType Leaf)) {
  throw "Expected installer '$installerPath' was not generated."
}

# Future code signing belongs here, before the checksum is generated.
$hash = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash
$hashLine = "$($hash.ToLowerInvariant())  $([System.IO.Path]::GetFileName($installerPath))"
Set-Content -LiteralPath $hashPath -Value $hashLine -Encoding ASCII

Remove-Item -LiteralPath $stagingRoot -Recurse -Force

Write-Output "Windows release package created:"
Write-Output "  $installerPath"
Write-Output "  $hashPath"
Write-Warning "This installer is unsigned and may trigger Windows SmartScreen."
Write-Output "Create tag v$version, then upload both files to GitHub Releases."
