# Launches the published app on generated sample pictures and saves screenshots of its window (dark photo view, dark gallery, light photo view).
param([string]$Exe = "publish\win-x64\FeatherView.exe", [string]$Out = "..\screenshots")
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W32 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$scr = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
Write-Host "Screen: $($scr.Width) x $($scr.Height)"

# ---- sample pictures: simple landscapes in different moods
$dir = Join-Path $env:RUNNER_TEMP "fv-shots\Holiday 2026"; New-Item -ItemType Directory -Force $dir | Out-Null
function New-Scene($path, $top, $bottom, $sun, $mount, $water, [bool]$gif = $false) {
  $w = 1600; $h = 1000
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.SmoothingMode = "AntiAlias"
  $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
  $sky = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $top, $bottom, 90
  $g.FillRectangle($sky, $rect)
  $g.FillEllipse((New-Object System.Drawing.SolidBrush $sun), 1000, 330, 140, 140)
  $far = [System.Drawing.Point[]]@((New-Object System.Drawing.Point 0,620), (New-Object System.Drawing.Point 250,430), (New-Object System.Drawing.Point 520,560), (New-Object System.Drawing.Point 860,360), (New-Object System.Drawing.Point 1250,590), (New-Object System.Drawing.Point 1600,450), (New-Object System.Drawing.Point 1600,700), (New-Object System.Drawing.Point 0,700))
  $g.FillPolygon((New-Object System.Drawing.SolidBrush $mount), $far)
  $g.FillRectangle((New-Object System.Drawing.SolidBrush $water), 0, 700, $w, 300)
  $g.Dispose()
  $fmt = if ($gif) { [System.Drawing.Imaging.ImageFormat]::Gif } elseif ($path.EndsWith(".png")) { [System.Drawing.Imaging.ImageFormat]::Png } else { [System.Drawing.Imaging.ImageFormat]::Jpeg }
  $bmp.Save($path, $fmt); $bmp.Dispose()
}
function C($r,$g,$b) { [System.Drawing.Color]::FromArgb($r,$g,$b) }
New-Scene "$dir\mountain-lake.jpg"   (C 70 120 200)  (C 250 190 140) (C 255 240 200) (C 40 60 70)  (C 40 100 120)
New-Scene "$dir\sunset-beach.jpg"    (C 240 120 70)  (C 255 200 110) (C 255 250 220) (C 120 50 60) (C 190 100 80)
New-Scene "$dir\blue-hour.jpg"       (C 20 30 80)    (C 90 130 200)  (C 230 235 255) (C 15 20 40)  (C 30 60 110)
New-Scene "$dir\forest-morning.jpg"  (C 150 210 190) (C 240 245 200) (C 255 255 230) (C 30 90 60)  (C 60 130 110)
New-Scene "$dir\desert-dusk.png"     (C 200 90 110)  (C 250 170 100) (C 255 220 170) (C 110 50 40) (C 170 110 80)
New-Scene "$dir\alpine-lake.jpg"     (C 100 160 230) (C 200 230 250) (C 255 255 255) (C 70 90 110) (C 50 110 150)
New-Scene "$dir\golden-hills.jpg"    (C 230 170 80)  (C 255 225 150) (C 255 245 200) (C 130 100 40) (C 90 120 90)
New-Scene "$dir\storm-coast.jpg"     (C 60 70 85)    (C 140 150 160) (C 220 225 230) (C 30 40 50)  (C 50 80 100)
New-Scene "$dir\cherry-spring.png"   (C 250 200 220) (C 255 240 245) (C 255 255 240) (C 180 100 130)(C 150 190 210)
New-Scene "$dir\night-lake.jpg"      (C 10 15 40)    (C 40 50 90)    (C 240 240 220) (C 10 12 25)  (C 20 40 70)
New-Scene "$dir\autumn-valley.jpg"   (C 120 160 210) (C 250 220 170) (C 255 245 210) (C 150 70 30) (C 80 110 100)
New-Scene "$dir\snow-peak.gif"       (C 150 190 240) (C 235 245 255) (C 255 255 255) (C 220 230 240)(C 120 160 200) $true

$settingsDir = Join-Path $env:APPDATA "FeatherView"; New-Item -ItemType Directory -Force $settingsDir | Out-Null
function Set-Theme($theme) {
  $json = @{ Theme = $theme; RestoreSession = $false; AutoUpdateCheck = $false; RememberWindow = $false; ShowInfoPanelOnOpen = $true; ShowFilmstrip = $true } | ConvertTo-Json
  Set-Content -Path (Join-Path $settingsDir "settings.json") -Value $json -Encoding UTF8
  Remove-Item (Join-Path $settingsDir "session.json") -ErrorAction SilentlyContinue
}
function Take($name, $arg, $waitSeconds) {
  $p = Start-Process -FilePath $Exe -ArgumentList "`"$arg`"" -PassThru
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  while ($p.MainWindowHandle -eq 0 -and $sw.Elapsed.TotalSeconds -lt 20) { Start-Sleep -Milliseconds 100; $p.Refresh() }
  if ($p.MainWindowHandle -eq 0) { Write-Host "No window for $name"; try { $p.Kill() } catch {}; return }
  $w = [Math]::Min(1280, $scr.Width); $h = [Math]::Min(800, $scr.Height)
  [W32]::MoveWindow($p.MainWindowHandle, 0, 0, $w, $h, $true) | Out-Null
  [W32]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
  Start-Sleep -Seconds $waitSeconds
  $r = New-Object W32+RECT; [W32]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
  $cw = [Math]::Max(10, [Math]::Min($r.R, $scr.Width) - [Math]::Max($r.L, 0)); $ch = [Math]::Max(10, [Math]::Min($r.B, $scr.Height) - [Math]::Max($r.T, 0))
  $bmp = New-Object System.Drawing.Bitmap $cw, $ch
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen([Math]::Max($r.L, 0), [Math]::Max($r.T, 0), 0, 0, $bmp.Size)
  $g.Dispose(); $bmp.Save((Join-Path $Out "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  Write-Host "Saved $name ($cw x $ch)"
  $log = Join-Path $settingsDir "featherview.log"
  if (Test-Path $log) { Copy-Item $log (Join-Path $Out "featherview-$name.log") -Force; Remove-Item $log -Force }
  try { $p.CloseMainWindow() | Out-Null; if (-not $p.WaitForExit(4000)) { $p.Kill() } } catch {}
}
Remove-Item (Join-Path $settingsDir "featherview.log") -ErrorAction SilentlyContinue
function Capture-Screen($name) {
  $bmp = New-Object System.Drawing.Bitmap $scr.Width, $scr.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen(0, 0, 0, 0, $bmp.Size); $g.Dispose()
  $bmp.Save((Join-Path $Out "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
}
# Full screen test: F11 on a picture, whole-screen shot, then Esc and another whole-screen shot
function Test-Fullscreen($arg) {
  $p = Start-Process -FilePath $Exe -ArgumentList "`"$arg`"" -PassThru
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  while ($p.MainWindowHandle -eq 0 -and $sw.Elapsed.TotalSeconds -lt 20) { Start-Sleep -Milliseconds 100; $p.Refresh() }
  Start-Sleep -Seconds 4
  [W32]::SetForegroundWindow($p.MainWindowHandle) | Out-Null; Start-Sleep -Milliseconds 500
  [System.Windows.Forms.SendKeys]::SendWait("{F11}"); Start-Sleep -Seconds 3
  $r = New-Object W32+RECT; [W32]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
  Write-Host "After F11: window $($r.L),$($r.T) - $($r.R),$($r.B); screen $($scr.Width) x $($scr.Height)"
  "After F11: window $($r.L),$($r.T) - $($r.R),$($r.B); screen $($scr.Width) x $($scr.Height)" | Out-File (Join-Path $Out "fullscreen-check.txt")
  Capture-Screen "photo-fullscreen"
  [System.Windows.Forms.SendKeys]::SendWait("{ESC}"); Start-Sleep -Seconds 3
  [W32]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
  "After Esc: window $($r.L),$($r.T) - $($r.R),$($r.B)" | Out-File (Join-Path $Out "fullscreen-check.txt") -Append
  Capture-Screen "photo-after-esc"
  try { $p.CloseMainWindow() | Out-Null; if (-not $p.WaitForExit(4000)) { $p.Kill() } } catch {}
}
Set-Theme "Dark"
Test-Fullscreen "$dir\mountain-lake.jpg"
Take "photo-dark" "$dir\mountain-lake.jpg" 6
Take "gallery-dark" $dir 9
# Pictures only in sub-folders (like opening a whole drive): opening the parent must show all of them at once
$tree = Join-Path $env:RUNNER_TEMP "fv-shots\All Pictures"
foreach ($sub in @("Italy\Rome", "Japan", "Family\2026\Summer")) { New-Item -ItemType Directory -Force (Join-Path $tree $sub) | Out-Null }
$files = Get-ChildItem $dir -File
for ($i = 0; $i -lt $files.Count; $i++) {
  $target = @("Italy\Rome", "Japan", "Family\2026\Summer")[$i % 3]
  Copy-Item $files[$i].FullName (Join-Path (Join-Path $tree $target) $files[$i].Name)
}
Take "gallery-tree" $tree 10
Set-Theme "Light"
Take "photo-light" "$dir\sunset-beach.jpg" 6
