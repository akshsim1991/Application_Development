# FileGlade — handoff notes (written 6 Nov 2026, before a 4–5 day pause)

Owner/designer: Akshaya Simha. Free forever: no ads, accounts, subscriptions or telemetry.

## Where things are
- Mac app: this folder (`FileGlade-Mac/` in the Application_Development repo). Build: `./build.sh` (needs the Xcode licence accepted). Tests: `./FileGlade.app/Contents/MacOS/FileGlade --self-test`, `./test-dashboard.sh`, `./test-power-tools.sh`.
- iPhone app: `FileGlade-iOS/` (Xcode project). Build for simulator without signing: `xcodebuild -project FileGlade.xcodeproj -scheme FileGlade -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build`. New Swift files are added to the project with `python3 add_to_project.py File.swift`.
- Versions: `VERSION` + automatic build number (`.build-number`); `./bump-version.sh [major|minor|patch]` also updates the iPhone project. `./release.sh` bumps, builds and prepares `release/` (zip + latest.json) for the public repo `akshsim1991/FileGlade-releases` — it uploads nothing.
- Update channel: the app reads `latest.json` from https://raw.githubusercontent.com/akshsim1991/FileGlade-releases/main/ . Published there now: 3.0 only. Newer builds exist only in this repo.
- Developer test flags (Mac): `--self-test`, `--snapshot <png> [--theme x] [--glass] [--dual] [--details] [--size WxH] <folder>`, `--bench-select <folder>`, `--test-doubleclick <folder> [--single] [--empty-area]`.

## State at the pause
Done and tested (Mac): tabs, dual pane, basket, compare/sync, advanced rename, archives (bsdtar), document search (Spotlight), shortcuts (Mac/Windows layouts), auto-update, automation rules, tags/notes, collections, storage analyzer, similar photos, backup/restore, themes (Colourful/Dark/AMOLED/Glass), colourful icons, resizable panes/columns, adaptive toolbar, ascending/descending sort, double-click fix, USB bridge client + protocol test, transfer journal/recovery/verification/pause (see below).

Built but NOT tested with real hardware: iPhone/iPad in the sidebar (ImageCaptureCore), usbmuxd connection, iPhone-side bridge server (compiles; protocol tested over a local socket on the Mac only), PhotoKit serving, Glass blur look (snapshots cannot show the blur).

## Not done yet (next steps, in order)
1. (Done in 3.3.0) Transfer journal, recovery, verification and pause/resume are tested in `--self-test`.
2. (Mostly done in 3.5.0 — see CHANGELOG; the iPhone app builds but has not been run on a device: scanner, Markup, Photos serving and the USB bridge need a real iPhone to verify.) Original list: iPhone Files-app parity: Quick Look pager with Markup, document scanner (VisionKit) with OCR PDF, tags + folder colour/symbol UI (data layer exists in PowerTools `FileLibrary`), Browse hub (Favorites, Tags, Recents), Recently Deleted improvements, iCloud download/remove-download, drag and drop, Move to…, Open-in document types (Info.plist already declares them; needs `onOpenURL` import).
3. (Done in 3.4.0: regex/boolean search, hex viewer, text compare, empty-folder finder, search export, file signatures.) Remaining spec P1 (see the PDF "Free Cross-Platform File Manager Feature Specification", 335 features): regex/boolean search, hex viewer, text compare, similar filenames, search export, empty-folder finder.
4. Things that cannot be done without extra pieces: iPhone SMB (needs a library), Windows version (needs a Windows machine and a different stack), notarised distribution (needs a paid Apple Developer ID).

## Known caveats
- iOS never lets an app list the whole phone. FileGlade can only share its own folders, folders you grant, and Photos.
- The release zip is signed with a development certificate: it runs on the owner's Mac; other Macs need notarisation.
- Full Disk Access must be granted to FileGlade once (menu FileGlade ▸ Full Disk Access Settings…).
