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
public static extern bool SetForegroundWindow(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool SetCursorPos(int x, int y);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, IntPtr dwExtraInfo);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern IntPtr SendMessageW(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern int GetWindowTextLengthW(IntPtr hWnd);
public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
public struct RECT { public int Left, Top, Right, Bottom; }
"@

$WM_NCHITTEST = 0x0084
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

$rect = New-Object Win32.Native+RECT
[Win32.Native]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
Write-Output ("HWND=0x{0:X} L={1} T={2} w={3} h={4} IsIconic={5}" -f $hwnd.ToInt64(),$rect.Left,$rect.Top,($rect.Right-$rect.Left),($rect.Bottom-$rect.Top),[Win32.Native]::IsIconic($hwnd))

[Win32.Native]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 400

function Click($physX, $physY) {
  [Win32.Native]::SetCursorPos($physX, $physY) | Out-Null
  Start-Sleep -Milliseconds 150
  [Win32.Native]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)  # LEFTDOWN
  Start-Sleep -Milliseconds 60
  [Win32.Native]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)  # LEFTUP
  Start-Sleep -Milliseconds 400
}

function State() {
  $r = New-Object Win32.Native+RECT
  [Win32.Native]::GetWindowRect($hwnd, [ref]$r) | Out-Null
  $lparam = (($r.Top + 16 -band 0xFFFF) -shl 16) -bor (($r.Left + 40) -band 0xFFFF)
  $ht = [Win32.Native]::SendMessageW($hwnd, $WM_NCHITTEST, [IntPtr]::Zero, [IntPtr]$lparam).ToInt64()
  Write-Output ("  -> IsIconic={0} IsZoomed={1} RECT=L{2} T{3} W{4} H{5} hit@caption-left={6}" -f [Win32.Native]::IsIconic($hwnd), [Win32.Native]::IsZoomed($hwnd), $r.Left,$r.Top,($r.Right-$r.Left),($r.Bottom-$r.Top), $ht)
}

# Sidebar nav item positions (logical px, dpr=1.0 for this window): x=40, y=120/160/200/240
$targets = @(
  @{ name="sidebar_nav1"; lx=40; ly=120 },
  @{ name="sidebar_nav2"; lx=40; ly=160 },
  @{ name="sidebar_nav3"; lx=40; ly=200 },
  @{ name="sidebar_nav4"; lx=40; ly=240 },
  @{ name="caption_title"; lx=60; ly=20 }
)

foreach ($t in $targets) {
  $px = $rect.Left + $t.lx
  $py = $rect.Top  + $t.ly
  Write-Output ("BEFORE click {0} at log({1},{2}) phys({3},{4})" -f $t.name, $t.lx, $t.ly, $px, $py)
  State
  Click $px $py
  Write-Output "AFTER click:"
  State
}
