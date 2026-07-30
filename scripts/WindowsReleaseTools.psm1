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
