# Roadmap vs. the "Advanced Windows Optimization & Fast Operation Manager" brief
Status key: DONE = real, implemented. PART = partly implemented. NEXT = planned, not started. Every item is built only when it performs real Windows operations (no placeholders). Nothing here has been run on real Windows yet; see docs/TEST_REPORT.md.

| # | Module | Status | Notes |
|---|---|---|---|
| 2 | System cleaner | PART (1.8) | Done: user/Windows temp, **Windows Update download cache** (stops/restarts wuauserv+BITS, skipped while updates install), Delivery Optimization, DirectX shader cache, Windows internet cache, thumbnails, error reports, user crash dumps, system memory dumps, old Windows logs, recent-files history, prefetch (30+ days, medium risk), font cache (service-assisted, medium), Edge/Chrome/Brave/Firefox cache, Recycle Bin, DNS cache flush, Windows.old detection that opens Disk Cleanup (high risk). Not done: clipboard history, installer package cache (unsafe), scheduled run. Risk column and per-item selection, sizes, file counts, logs (History page) are in. |
| 3 | Temp cleaner extras | NEXT | custom folders, exclusion list, scheduled cleaning, detailed report export (export exists) |
| 4 | Registry cleaner | PART | 4 conservative areas with backup/restore. NEXT: broken file associations, stale COM/ActiveX (needs careful rules) |
| 5-6 | Driver cleaner / updater | NEXT | needs SetupAPI/PnPUtil + Windows Update Agent integration; read-only driver list exists |
| 7 | Full uninstaller | NEXT (high value) | list from Uninstall keys, run official/quiet uninstaller, leftover detection by install path, associated startup/service/task detection (existing scanners reusable) |
| 8 | Windows Apps manager | NEXT | PowerShell Appx cmdlets (Windows 10/11 only) |
| 9 | Duplicate finder | PART (1.7) | SHA-256, Recycle Bin, keep-one rule. NEXT: quarantine folder, ignore lists, max size, report |
| 10-11 | Disk analyzer / storage optimizer | NEXT | treemap needs a custom-drawn control; TRIM/SSD via DeviceIoControl; Optimize Drives via defrag.exe /O |
| 12 | System repair (SFC/DISM) | NEXT | run elevated sfc/DISM with captured output + cancel |
| 13, 17 | Performance / memory | NEXT | read-only analysis first; power plans via powercfg; no RAM "purging" |
| 14, 15 | Services / Startup | DONE (existing) | continuously improved |
| 16 | Process manager | NEXT | |
| 18 | Network tools | NEXT | flush DNS exists in Cleaner |
| 19 | Privacy manager | NEXT | only supported registry/policy settings with restore |
| 20 | File shredder | NEXT | with SSD/cloud warning |
| 21 | Large file finder | DONE (1.7) | |
| 22 | Empty folder finder | DONE (1.8) | |
| 23 | Browser cleaner (history/cookies) | NEXT | cache part exists in Cleaner |
| 24 | Context menu manager | PART | Explorer add-ons are listed and can be disabled on the Startup page |
| 25 | Scheduled maintenance | NEXT | Task Scheduler with consent (delay tasks already work this way) |
| 26-30 | System info / backup / restore points / Windows Update manager / security check | NEXT | |
| 31-33 | UI / safety engine | PART | themes, sidebar, journal, backups, confirmations, risk labels in Cleaner; central safety validator is partial |

## Platform support
- Windows 10 and 11 x64: primary targets.
- **Windows 7 SP1 (x64 and x86) and Windows 8/8.1**: 1.8 builds an x86 exe and compiles both builds against the Windows 7 API level (`_WIN32_WINNT=0x0601`); Windows 10-only features (per-monitor DPI, dark popup menus, per-user services) are detected at run time and skipped. Not yet tested on a real Windows 7 machine. Windows 7 has no Store apps, so the future Apps manager will not exist there.
