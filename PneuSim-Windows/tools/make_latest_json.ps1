# Creates latest.json for the public PneuSim-releases repository.
# Usage: pwsh tools/make_latest_json.ps1 -Installer PneuSim-Setup-3.3.0.exe -Version 3.3.0 -CodeName Mistral `
#          -Url https://github.com/akshsim1991/PneuSim-releases/releases/download/v3.3.0/PneuSim-Setup-3.3.0.exe -Notes "line 1\nline 2"
param([Parameter(Mandatory)][string]$Installer, [Parameter(Mandatory)][string]$Version, [Parameter(Mandatory)][string]$CodeName,
      [Parameter(Mandatory)][string]$Url, [string]$Notes = "", [string]$Out = "latest.json")
if (-not $Url.StartsWith("https://")) { throw "The installer URL must start with https://" }
$sha = (Get-FileHash -Algorithm SHA256 $Installer).Hash.ToLower()
$obj = [ordered]@{ version = $Version; codename = $CodeName; released = (Get-Date -Format "yyyy-MM-dd"); installerUrl = $Url; sha256 = $sha; notes = $Notes }
$obj | ConvertTo-Json | Set-Content -Encoding UTF8 $Out
Write-Host "Wrote $Out (sha256 $sha)"
