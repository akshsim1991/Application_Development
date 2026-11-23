# Release-build smoke test: launches the app on a generated image and records startup time, memory and responsiveness.
param([string]$Exe = "publish\win-x64\FeatherView.exe")
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$dir = Join-Path $env:RUNNER_TEMP "fv-smoke"; New-Item -ItemType Directory -Force $dir | Out-Null
foreach ($i in 1..3) {
  $bmp = New-Object System.Drawing.Bitmap 1600, 1000
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.Clear([System.Drawing.Color]::FromArgb(40 * $i, 90, 200 - 40 * $i))
  $bmp.Save((Join-Path $dir "test$i.png"), [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
}
function Show-Diagnostics {
  Write-Host "---- app log ----"
  $log = Join-Path $env:APPDATA "FeatherView\featherview.log"
  if (Test-Path $log) { Get-Content $log -Tail 60 | ForEach-Object { Write-Host $_ } } else { Write-Host "(no log file was written)" }
  Write-Host "---- processes ----"
  Get-Process FeatherView -ErrorAction SilentlyContinue | ForEach-Object { Write-Host ("pid {0} title '{1}' handle {2} responding {3}" -f $_.Id, $_.MainWindowTitle, $_.MainWindowHandle, $_.Responding) }
}
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $Exe -ArgumentList "`"$dir\test1.png`"" -PassThru
$p.Refresh()
while ($p.MainWindowHandle -eq 0 -and $sw.Elapsed.TotalSeconds -lt 20) { Start-Sleep -Milliseconds 20; $p.Refresh() }
$startMs = [int]$sw.ElapsedMilliseconds
if ($p.HasExited) { Show-Diagnostics; throw "The app exited during start-up (exit code $($p.ExitCode))." }
if ($p.MainWindowHandle -eq 0) { Show-Diagnostics; try { $p.Kill() } catch {}; throw "No window appeared within 20 s." }
Start-Sleep -Seconds 3
$p.Refresh()
$mem = [math]::Round($p.WorkingSet64 / 1MB, 1)
$cpu = [math]::Round($p.TotalProcessorTime.TotalSeconds, 2)
$responding = $p.Responding
$p.CloseMainWindow() | Out-Null
if (-not $p.WaitForExit(5000)) { $p.Kill() }
$line = "Window after $startMs ms | working set $mem MB | CPU time $cpu s | responding: $responding"
Write-Host $line
if ($env:GITHUB_STEP_SUMMARY) { "### Release smoke test`n$line" | Out-File -Append $env:GITHUB_STEP_SUMMARY }
if (-not $responding) { Show-Diagnostics; throw "The app stopped responding." }
