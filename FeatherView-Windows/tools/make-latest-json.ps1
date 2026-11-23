# Writes dist\latest.json (attached to the GitHub Release, which the app reads through .../releases/latest/download/latest.json):
# version, notes, installer URL, size and SHA-256 - the same scheme FeatherPDF and FileGlade use.
param([string]$Notes = "")
if (-not $Notes -and (Test-Path "release-notes.txt")) { $Notes = (Get-Content "release-notes.txt" -Raw).Trim() }
if (-not $Notes) { $Notes = "Improvements and fixes." }
$v = (Get-Content VERSION).Trim()
$setup = "dist\FeatherView-Setup-$v.exe"
$hash = (Get-FileHash $setup -Algorithm SHA256).Hash.ToLower()
$size = (Get-Item $setup).Length
$json = [ordered]@{
  version = $v
  date    = (Get-Date -Format "yyyy-MM-dd")
  notes   = $Notes
  win64   = [ordered]@{
    url    = "https://github.com/akshsim1991/Application_Development/releases/download/featherview-v$v/FeatherView-Setup-$v.exe"
    size   = $size
    sha256 = $hash
  }
} | ConvertTo-Json -Depth 4
$json | Out-File -Encoding utf8 dist\latest.json
Write-Host $json
