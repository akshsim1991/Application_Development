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

# Extended video engine (libmpv) package

Feather View can play more formats with the open-source libmpv library. It is **not bundled**; the user downloads it from Settings > Video (or the player's notice bar), or copies `libmpv-2.dll` into `%LOCALAPPDATA%\FeatherView\engine\` (or an `engine` folder next to the app for portable use).

Publish `engine.json` at the root of `akshsim1991/FeatherView-releases` (branch `main`) and attach the ZIP to a release:
```json
{ "version": "0.37.0", "license": "LGPL-2.1-or-later", "notes": "mpv/FFmpeg build",
  "win64": { "url": "https://github.com/akshsim1991/FeatherView-releases/releases/download/engine-0.37.0/libmpv-win64.zip",
             "size": 31234567, "sha256": "<hex>", "file": "libmpv-2.dll" } }
```
Rules: the ZIP must contain the named DLL (any folder) and may contain LICENSE/COPYING/README text files; nothing else is extracted. **Use an LGPL build (not GPL), keep dynamic linking, ship the licence texts and a source offer.** Codec patent and licence obligations differ by country; get legal review before publishing.
