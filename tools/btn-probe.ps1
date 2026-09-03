Add-Type -Namespace Win32 -Name Native -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsWindowVisible(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern uint GetDpiForWindow(IntPtr hWnd);
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
$dpi = [Win32.Native]::GetDpiForWindow($hwnd)
$dpr = $dpi / 96.0
$physW = $rect.Right - $rect.Left
$logW = $physW / $dpr
Write-Output ("HWND=0x{0:X} L={1} T={2} physW={3} dpi={4} dpr={5} logW={6}" -f $hwnd.ToInt64(),$rect.Left,$rect.Top,$physW,$dpi,$dpr,$logW)

# Button visual logical positions: each 46 wide, from right. caption height 32.
# minimize: logX [logW-138, logW-92], maximize: [logW-92, logW-46], close: [logW-46, logW]
$minL = $logW - 138; $maxL = $logW - 92; $closeL = $logW - 46
Write-Output ("button visual logical: minimize[{0},{1}] maximize[{2},{3}] close[{4},{5}]" -f $minL,($minL+46),$maxL,($maxL+46),$closeL,($closeL+46))

function HitName($code) {
  switch ($code) { 1 {"CLIENT"} 8 {"MIN"} 9 {"MAX"} 20 {"CLOSE"} 10 {"LEFT"} 11 {"RIGHT"} 12 {"TOP"} 13 {"TL"} 14 {"TR"} 2 {"CAP"} default {"X$code"} }
}

# Sample across caption row at physical y=16 (top area), physical x covering right 300px, every 4 phys px.
Write-Output "=== hit-test across caption (phys y=20) ==="
$yPhys = $rect.Top + 20
for ($xPhys = $rect.Right - 300; $xPhys -le $rect.Right; $xPhys += 4) {
  $lx = ($xPhys - $rect.Left) / $dpr
  $lparam = (($yPhys -band 0xFFFF) -shl 16) -bor ($xPhys -band 0xFFFF)
  $code = [Win32.Native]::SendMessageW($hwnd, $WM_NCHITTEST, [IntPtr]::Zero, [IntPtr]$lparam).ToInt64()
  Write-Output ("physX={0,4} logX={1,6:F1} -> {2}" -f $xPhys, $lx, (HitName $code))
}
