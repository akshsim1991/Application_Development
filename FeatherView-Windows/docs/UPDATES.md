# Releasing an update

Installed copies read `latest.json` from the newest normal (not pre-release) GitHub Release of this repository:
`https://github.com/akshsim1991/Application_Development/releases/latest/download/latest.json`.

To publish an update:
1. Put the new version number in `VERSION` and `Version` in `src/FeatherView.App/FeatherView.App.csproj` (it must be higher than the installed one) and write the changes in `release-notes.txt`.
2. Push with `[release]` in the commit message. CI builds the installer, creates `latest.json` (version, notes, installer URL, size, SHA-256) and publishes the release `featherview-v<version>` with the installer, ZIPs and `latest.json`.
3. Installed copies notice it within a day (5 seconds after start-up, at most once every 24 hours) or at once through Settings > Updates > Check for updates now. They ask first, download from github.com, verify size and SHA-256, then run the installer silently and restart.

Copies older than 0.1.1 looked at a different address and cannot update themselves: install 0.1.1 once by hand.

`latest.json`:
```json
{ "version": "0.1.1", "date": "2026-11-23", "notes": "What's new",
  "win64": { "url": "https://github.com/akshsim1991/Application_Development/releases/download/featherview-v0.1.1/FeatherView-Setup-0.1.1.exe", "size": 45655590, "sha256": "<hex>" } }
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
