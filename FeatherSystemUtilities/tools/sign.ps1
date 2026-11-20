# Signs a Windows binary with signtool (Windows SDK). Usage: .\tools\sign.ps1 -File build\FeatherSystemUtilities.exe -Pfx cert.pfx -Password ...
param([Parameter(Mandatory)][string]$File, [Parameter(Mandatory)][string]$Pfx, [Parameter(Mandatory)][string]$Password, [string]$Timestamp = "http://timestamp.digicert.com")
signtool sign /f $Pfx /p $Password /fd SHA256 /tr $Timestamp /td SHA256 /d "Feather System Utilities" $File
signtool verify /pa /v $File
