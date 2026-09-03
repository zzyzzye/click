Add-Type -Namespace Win32 -Name Native -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsIconic(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsZoomed(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsWindowVisible(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool SetForegroundWindow(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool SetCursorPos(int x, int y);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, IntPtr dwExtraInfo);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern IntPtr GetForegroundWindow();
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern uint GetWindowThreadProcessId(IntPtr hWnd, IntPtr lpdwProcessId);
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern uint GetCurrentThreadId();
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool AttachThreadInput(uint idAttach, uint idAttachTo, bool fAttach);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern int GetWindowTextLengthW(IntPtr hWnd);
public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
public struct RECT { public int Left, Top, Right, Bottom; }
"@

$cfPids = (Get-Process -Name ClickFlow -ErrorAction SilentlyContinue).Id
$hwnd = [IntPtr]::Zero
$enumProc = [Win32.Native+EnumWindowsProc]{
  param($h, $lp)
  $procId = 0
  [Win32.Native]::GetWindowThreadProcessId($h, [ref]$procId) | Out-Null
  if ($cfPids -contains $procId -and [Win32.Native]::IsWindowVisible($h) -and ([Win32.Native]::GetWindowTextLengthW($h) -gt 0) -and ($script:hwnd -eq [IntPtr]::Zero)) { $script:hwnd = $h }
  return $true
}
[Win32.Native]::EnumWindows($enumProc, [IntPtr]::Zero) | Out-Null
if ($hwnd -eq [IntPtr]::Zero) { Write-Output "NO_WINDOW"; exit 1 }

function ForceForeground($hwnd) {
  $fg = [Win32.Native]::GetForegroundWindow()
  $fgTid = [Win32.Native]::GetWindowThreadProcessId($fg, [IntPtr]::Zero)
  $myTid = [Win32.Native]::GetCurrentThreadId()
  [Win32.Native]::AttachThreadInput($myTid, $fgTid, $true) | Out-Null
  [Win32.Native]::SetForegroundWindow($hwnd) | Out-Null
  [Win32.Native]::AttachThreadInput($myTid, $fgTid, $false) | Out-Null
}

function Click($physX, $physY) {
  [Win32.Native]::SetCursorPos($physX, $physY) | Out-Null
  Start-Sleep -Milliseconds 120
  [Win32.Native]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 50
  [Win32.Native]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 350
}

function RestoreIfNeeded($hwnd) {
  if ([Win32.Native]::IsIconic($hwnd)) {
    [Win32.Native]::ShowWindow($hwnd, 9) | Out-Null  # SW_RESTORE
    Start-Sleep -Milliseconds 300
  }
}

$rect = New-Object Win32.Native+RECT
[Win32.Native]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
Write-Output ("HWND=0x{0:X} L={1} T={2} W={3} H={4}" -f $hwnd.ToInt64(),$rect.Left,$rect.Top,($rect.Right-$rect.Left),($rect.Bottom-$rect.Top))

# Sanity: click minimize button visual center (logX ~621, logY 16, dpr1.25 -> phys)
$dpr = 1.25
$minCx = [int]($rect.Left + 621 * $dpr)
$minCy = [int]($rect.Top + 16 * $dpr)
ForceForeground $hwnd
Start-Sleep -Milliseconds 300
Write-Output "SANITY: click minimize button visual center"
Click $minCx $minCy
$iconic = [Win32.Native]::IsIconic($hwnd)
Write-Output ("  after click minimize: IsIconic={0} (expect True if sim works & btn works)" -f $iconic)
RestoreIfNeeded $hwnd
if (-not $iconic) {
  # maybe button hittest shifted; try the MAX hittest region center logX ~680
  $maxCx = [int]($rect.Left + 680 * $dpr)
  Write-Output "SANITY2: click MAX hittest region center logX~680"
  Click $maxCx $minCy
  Write-Output ("  after click MAX region: IsIconic={0} IsZoomed={1}" -f [Win32.Native]::IsIconic($hwnd), [Win32.Native]::IsZoomed($hwnd))
  RestoreIfNeeded $hwnd
}

# Grid click across whole window (logical step 48), report any click that triggers minimize.
Write-Output "=== grid click scan (logical step 48) ==="
[Win32.Native]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$logW = ($rect.Right - $rect.Left) / $dpr
$logH = ($rect.Bottom - $rect.Top) / $dpr
for ($ly = 8; $ly -lt $logH; $ly += 48) {
  for ($lx = 8; $lx -lt $logW; $lx += 48) {
    RestoreIfNeeded $hwnd
    ForceForeground $hwnd
    Start-Sleep -Milliseconds 80
    $px = [int]($rect.Left + $lx * $dpr)
    $py = [int]($rect.Top + $ly * $dpr)
    Click $px $py
    if ([Win32.Native]::IsIconic($hwnd)) {
      Write-Output ("*** MINIMIZED at log({0},{1}) phys({2},{3}) ***" -f $lx,$ly,$px,$py)
    }
  }
}
Write-Output "=== scan done ==="
