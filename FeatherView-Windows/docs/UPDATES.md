# Version numbers

Feather View uses MAJOR.MINOR.PATCH and starts at **2.1.0** (matching the V2.1 roadmap). Bug fixes are 2.1.1, 2.1.2, ...; the next feature release is 2.2.0; V3 becomes 3.0.0. The number must always increase, or installed copies will not see an update.

# Releasing an update

Installed copies read `latest.json` from the newest normal (not pre-release) GitHub Release of this repository:
`https://github.com/akshsim1991/Application_Development/releases/latest/download/latest.json`.

To publish an update:
1. Put the new version number in `VERSION` and `Version` in `src/FeatherView.App/FeatherView.App.csproj` (it must be higher than the installed one) and write the changes in `release-notes.txt`.
2. Push with `[release]` in the commit message. CI builds the installer, creates `latest.json` (version, notes, installer URL, size, SHA-256) and publishes the release `featherview-v<version>` with the installer, ZIPs and `latest.json`.
3. Installed copies notice it within a day (5 seconds after start-up, at most once every 24 hours) or at once through Settings > Updates > Check for updates now. They ask first, download from github.com, verify size and SHA-256, then run the installer silently and restart.

Copies older than 2.1.0 (the early 0.1.x test builds) looked at a different address and cannot update themselves: install 2.1.0 once by hand.

`latest.json`:
```json
{ "version": "2.1.0", "date": "2026-11-23", "notes": "What's new",
  "win64": { "url": "https://github.com/akshsim1991/Application_Development/releases/download/featherview-v2.1.0/FeatherView-Setup-2.1.0.exe", "size": 45655590, "sha256": "<hex>" } }
```
Allowed hosts: raw.githubusercontent.com, github.com, objects.githubusercontent.com, release-assets.githubusercontent.com. Anything else is refused.

# Extended video engine (libmpv) package

Feather View plays more formats and gets audio tracks, frame stepping, A-B repeat, chapters and ASS subtitles with the open-source libmpv library. It is **not bundled**: the user downloads it from Settings > Video (or the player's notice bar), or copies `libmpv-2.dll` into `%LOCALAPPDATA%\FeatherView\engine\` (or an `engine` folder next to the app for portable use).

The download comes from the release `featherview-engine` of this repository (`engine.json` + `libmpv-win64.zip`). It is created by the build when a commit message contains `[engine]`: the workflow fetches an **LGPL** libmpv build from its upstream project, adds the licence texts, and publishes it with a SHA-256. The app checks size and SHA-256 and extracts only `libmpv-2.dll` and licence text files.

Rules: use an LGPL build (not GPL), keep dynamic linking, ship the licence texts and a source link. Codec patent and licence obligations differ by country; get legal review before wide distribution.
