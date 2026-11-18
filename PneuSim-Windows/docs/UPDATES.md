# Automatic updates

How it works (module M-UPDATE: `Core/UpdateService.cs`, `UI/UpdateDialog.cs`, `UI/MainForm.Update.cs`):
1. About once a day (and on **Help > Update to new version...**) PneuSim downloads `latest.json` from
   `https://raw.githubusercontent.com/akshsim1991/PneuSim-releases/main/latest.json`, with a mirror
   `https://cdn.jsdelivr.net/gh/akshsim1991/PneuSim-releases@main/latest.json` as fallback.
   No personal data is sent. Switch off in Settings.
2. If the version is newer, the **Update now / Later** window shows the notes. *Update now* downloads the
   installer (HTTPS only) to `%TEMP%\PneuSim-update`, checks the **SHA-256**, asks to save your work,
   runs the installer silently (same AppId, so it upgrades in place), closes PneuSim, and the installer
   starts the new version.
3. A download whose checksum does not match is deleted and never run.

## latest.json
```json
{ "version": "3.3.1", "codename": "Mistral", "released": "2026-12-01",
  "installerUrl": "https://github.com/akshsim1991/PneuSim-releases/releases/download/v3.3.1/PneuSim-Setup-3.3.1.exe",
  "sha256": "<64 hex characters>", "notes": "Fixed X\nAdded Y" }
```

## Publishing a release (owner)
1. Build the installer (CI artifact or `ISCC /DAppVersion=3.3.1 installer\PneuSim.iss`).
2. Create a GitHub release `v3.3.1` in the **public** repo `akshsim1991/PneuSim-releases` and attach the installer.
3. `pwsh tools/make_latest_json.ps1 -Installer <exe> -Version 3.3.1 -CodeName Mistral -Url <download url> -Notes "..."`
4. Commit the generated `latest.json` to the `main` branch of `PneuSim-releases`.
Security note: whoever can write to that repo can ship code to all users. Protect the account with 2FA;
code-signing the installer (when a certificate is bought) is the next hardening step.
