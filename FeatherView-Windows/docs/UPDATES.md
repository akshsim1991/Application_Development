# Releasing an update

1. Bump `VERSION` (and keep `Version` in `FeatherView.App.csproj` in step), push; CI builds `dist/FeatherView-Setup-<v>.exe` and `dist/latest.json`.
2. In the public repository `akshsim1991/FeatherView-releases` (create it once): publish a GitHub Release tagged `v<version>` with the installer attached,
   and commit `latest.json` to the repository root on `main`.
3. Installed copies pick it up within a day (or via Settings > Check for updates now).

`latest.json`:
```json
{ "version": "0.2.0", "date": "2026-12-01", "notes": "What's new",
  "win64": { "url": "https://github.com/akshsim1991/FeatherView-releases/releases/download/v0.2.0/FeatherView-Setup-0.2.0.exe", "size": 61234567, "sha256": "<hex>" } }
```
Allowed hosts: raw.githubusercontent.com, github.com, objects.githubusercontent.com, release-assets.githubusercontent.com. Anything else is refused.
