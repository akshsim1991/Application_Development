# GUI smoke test for PneuSim on Windows.
# Starts PneuSim.exe (optionally with a .pneu file), waits, checks that it is still running, that
# its main window has the expected title and that no error dialog ("Something went wrong …")
# appeared, then saves a screenshot. Exit code 0 = pass.
#
#   powershell -File gui-smoke.ps1 -Exe path\PneuSim.exe -Shot out.png [-File circuit.pneu] [-Seconds 12]
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$Shot,
    [string]$File = "",
    [int]$Seconds = 12
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public static class Win {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  public static List<string> Titles(uint pid) {
    var r = new List<string>();
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p);
      if (p == pid && IsWindowVisible(h)) { var t = new StringBuilder(512); var c = new StringBuilder(256);
        GetWindowText(h, t, 512); GetClassName(h, c, 256); r.Add(c + "|" + t); }
      return true; }, IntPtr.Zero);
    return r;
  }
}
"@

$argList = @()
if ($File -ne "") { $argList += "`"$File`"" }
$p = if ($argList.Count) { Start-Process -FilePath $Exe -ArgumentList $argList -PassThru } else { Start-Process -FilePath $Exe -PassThru }
Start-Sleep -Seconds $Seconds
$p.Refresh()
$fail = @()
if ($p.HasExited) { $fail += "process exited with code $($p.ExitCode)" }
else {
    $wins = [Win]::Titles([uint32]$p.Id)
    Write-Host "Windows:"; $wins | ForEach-Object { Write-Host "  $_" }
    if (-not ($wins | Where-Object { $_ -like "*PneuSim 3.*" })) { $fail += "main window with title 'PneuSim 3.x' not found" }
    # A standard MessageBox has window class #32770.
    $dialogs = $wins | Where-Object { $_ -like "#32770|*" }
    if ($dialogs) { $fail += "dialog open: $($dialogs -join '; ')" }
}
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
$bmp.Save($Shot, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Start-Sleep -Seconds 1
if ($fail) { Write-Host "FAIL: $($fail -join '; ')"; exit 1 }
Write-Host "PASS: $Exe $File"
exit 0
