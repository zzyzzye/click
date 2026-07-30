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

$installerLanguagePath = Join-Path $repositoryRoot `
  "installer\languages\ChineseSimplified.isl"
if (-not (Test-Path -LiteralPath $installerLanguagePath -PathType Leaf)) {
  throw "The vendored Inno Setup Simplified Chinese language file is missing."
}

Write-Output "Windows installer resource tests passed."
