# Writes dist\latest.json for the public releases repository (akshsim1991/FeatherView-releases),
# the same scheme FeatherPDF and FileGlade use: version, notes, installer URL, size and SHA-256.
param([string]$Notes = "Improvements and fixes.")
$v = (Get-Content VERSION).Trim()
$setup = "dist\FeatherView-Setup-$v.exe"
$hash = (Get-FileHash $setup -Algorithm SHA256).Hash.ToLower()
$size = (Get-Item $setup).Length
$json = [ordered]@{
  version = $v
  date    = (Get-Date -Format "yyyy-MM-dd")
  notes   = $Notes
  win64   = [ordered]@{
    url    = "https://github.com/akshsim1991/FeatherView-releases/releases/download/v$v/FeatherView-Setup-$v.exe"
    size   = $size
    sha256 = $hash
  }
} | ConvertTo-Json -Depth 4
$json | Out-File -Encoding utf8 dist\latest.json
Write-Host $json
