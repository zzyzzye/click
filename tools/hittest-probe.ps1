Add-Type -Namespace Win32 -Name Native -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsZoomed(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsIconic(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool IsWindowVisible(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern int GetWindowTextLengthW(IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern IntPtr SendMessageW(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
public struct RECT { public int Left, Top, Right, Bottom; }
"@

$WM_NCHITTEST = 0x0084
$cfPids = (Get-Process -Name ClickFlow -ErrorAction SilentlyContinue).Id

# Probe every ClickFlow top-level visible window.
$windows = New-Object System.Collections.ArrayList
$enumProc = [Win32.Native+EnumWindowsProc]{
  param($h, $lp)
  $procId = 0
  [Win32.Native]::GetWindowThreadProcessId($h, [ref]$procId) | Out-Null
  if ($cfPids -contains $procId -and [Win32.Native]::IsWindowVisible($h) -and ([Win32.Native]::GetWindowTextLengthW($h) -gt 0)) {
    $null = $windows.Add($h)
  }
  return $true
}
[Win32.Native]::EnumWindows($enumProc, [IntPtr]::Zero) | Out-Null

function HitName($code) {
  switch ($code) {
    1  { "CLIENT" } 2  { "CAPTION" } 3  { "SYSMENU" }
    8  { "MIN" } 9  { "MAX" }
    10 { "LEFT" } 11 { "RIGHT" } 12 { "TOP" }
    13 { "TOPLEFT" } 14 { "TOPRIGHT" } 15 { "BOTTOM" }
    16 { "BOTLEFT" } 17 { "BOTRIGHT" } 20 { "CLOSE" }
    default { "X$code" }
  }
}

foreach ($hwnd in $windows) {
  $rect = New-Object Win32.Native+RECT
  [Win32.Native]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
  Write-Output ("===== HWND=0x{0:X} RECT L={1} T={2} R={3} B={4} w={5} h={6} IsZoomed={7} =====" -f $hwnd.ToInt64(), $rect.Left,$rect.Top,$rect.Right,$rect.Bottom,($rect.Right-$rect.Left),($rect.Bottom-$rect.Top),[Win32.Native]::IsZoomed($hwnd))

  # Grid sample every 24 physical px; print rows of hit codes to see the whole map.
  for ($y = $rect.Top; $y -lt $rect.Bottom; $y += 24) {
    $line = ""
    for ($x = $rect.Left; $x -lt $rect.Right; $x += 24) {
      $lparam = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
      $code = [Win32.Native]::SendMessageW($hwnd, $WM_NCHITTEST, [IntPtr]::Zero, [IntPtr]$lparam).ToInt64()
      switch ($code) {
        1  { $line += "." }
        2  { $line += "c" }
        8  { $line += "m" }
        9  { $line += "M" }
        20 { $line += "X" }
        10 { $line += "L" }
        11 { $line += "R" }
        12 { $line += "T" }
        13 { $line += "7" }
        14 { $line += "9" }
        15 { $line += "_" }
        16 { $line += "J" }
        17 { $line += "U" }
        default { $line += "?" }
      }
    }
    Write-Output $line
  }
  Write-Output "legend: .=CLIENT c=CAPTION m=MIN M=MAX X=CLOSE L=LEFT R=RIGHT T=TOP 7=TL 9=TR _=BOT J=BL U=BR"
}
