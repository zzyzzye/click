Add-Type -Namespace Win32 -Name Native -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern IntPtr FindWindowW(string lpClassName, string lpWindowName);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsWindowVisible(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsWindow(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern IntPtr GetWindow(IntPtr hWnd, uint uCmd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern int GetWindowTextW(IntPtr hWnd, System.Text.StringBuilder lpString, int nMaxCount);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern int GetWindowTextLengthW(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
"@

# ClickFlow PIDs
$cfPids = (Get-Process -Name ClickFlow -ErrorAction SilentlyContinue).Id
Write-Output ("ClickFlow PIDs: " + ($cfPids -join ","))

$sb = New-Object System.Text.StringBuilder 512
$all = New-Object System.Collections.ArrayList
$enumProc = [Win32.Native+EnumWindowsProc]{
  param($h, $lp)
  $len = [Win32.Native]::GetWindowTextLengthW($h)
  $procId = 0
  [Win32.Native]::GetWindowThreadProcessId($h, [ref]$procId) | Out-Null
  if ($len -gt 0) {
    $sb.Clear() | Out-Null
    [Win32.Native]::GetWindowTextW($h, $sb, 512) | Out-Null
    $null = $all.Add(@{ hwnd=$h; title=$sb.ToString(); pid=$procId; vis=[Win32.Native]::IsWindowVisible($h) })
  }
  return $true
}
[Win32.Native]::EnumWindows($enumProc, [IntPtr]::Zero) | Out-Null

Write-Output "=== windows owned by ClickFlow PIDs ==="
foreach ($w in $all) {
  if ($cfPids -contains $w.pid) {
    Write-Output ("hwnd=0x{0:X} pid={1} vis={2} title='{3}'" -f $w.hwnd.ToInt64(), $w.pid, $w.vis, $w.title)
  }
}
Write-Output "=== first 30 visible titles ==="
$vis = $all | Where-Object { $_.vis } | Select-Object -First 30
foreach ($w in $vis) {
  Write-Output ("pid={0} title='{1}'" -f $w.pid, $w.title)
}
