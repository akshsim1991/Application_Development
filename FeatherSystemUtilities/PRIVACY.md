# Feather System Utilities — privacy policy and data inventory
Feather works offline. It has no accounts, no analytics and no telemetry. This page lists everything it stores and every network request it can make.

## What stays on your PC (`%LOCALAPPDATA%\FeatherSystemUtilities`)
| File / folder | Purpose | Delete it? |
|---|---|---|
| `settings.ini` | Your settings, window size, scan folders, exclusions, ignored findings | Yes (settings reset) |
| `journal.jsonl` | History of changes Feather made (Journal page) | Yes (Settings → Clear journal) |
| `backups`, `regbackups`, `taskbackups` | Exports made before registry/startup/task changes (Recovery → restore) | Yes, but you lose the undo |
| `quarantine` | Leftovers and stray files you removed, kept until you delete them | Yes (Recovery → Delete permanently) |
| `storage-trend.csv` | One line per drive per day (free/total space) for the trend chart | Yes |
| `firstseen.tsv` | When a startup entry was first seen (the "new" flag) | Yes |
| `advice.json` | Downloaded copy of the Startup/Services recommendation list | Yes |
| `license.token` | A license key you entered, if any | Yes (Settings → Remove) |
| `webview2` | The embedded web view's cache for Feather's own screens | Yes |

Reports you create (diagnostic report, CSV exports) are saved only where you choose and are never uploaded. Reports hide your user name, PC name, user-profile paths and SIDs by default.

## Network requests
Feather itself makes only these HTTPS requests, to the project's release pages on `raw.githubusercontent.com` / `cdn.jsdelivr.net`:
1. **Update check** at start (switch in Settings): downloads `latest.json`. The request carries the app name and version in the User-Agent. Updates are downloaded only after you approve them and are verified by size and SHA-256 (and by code signature once installers are signed).
2. **Recommendation list** (switch in Settings): downloads `advice.json`. Nothing about your PC is sent.
Other network use happens only when you ask for it, and is done by Windows or your browser, not by Feather: driver and Windows-update checks use Windows Update; "Look up online", "VirusTotal", maker-support and help links open your web browser after you confirm. For VirusTotal only a file hash is put in the link; the file is not uploaded.

## What Feather does not do
No tracking, no advertising, no selling or sharing of data, no remote access, no automatic changes to your system without your confirmation.

## Contact
See `SECURITY.md` to report a vulnerability. Developer: Akshaya Simha.
