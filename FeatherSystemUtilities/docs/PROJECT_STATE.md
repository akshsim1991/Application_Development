# Project state
Version 2.0.0 (2026-11-13). Branch `claude/new-session-ann5h4`. Release: `releases/v2.0.0` (setup + portable exe).
## Implemented (Wine-smoke-tested unless noted)
Shell (rail nav, native accessible toolbar buttons, themes x4, settings persistence, dashboard tiles, status bar, single instance, icon), Startup (Run/RunOnce, folders, tasks, add-ons; enable/disable, delete+backup, restore, add, undo, Start later/Remove delay [untested: Task Scheduler], multi-select, filters, hide system items), Services (inventory, start/stop/restart/pause/resume, kill, start type, profiles+snapshots with plan review, Properties + editors + History tab [untested]), Drivers (read-only), Cleaner (temp + conservative registry cleaning, backup/restore), Large files and Duplicate finders [compile-only], History, export CSV/HTML/JSON, legacy data copy, NSIS installer [untested], --cleanup for uninstall.
## Next
1. Owner test on Windows 11; fix issues. 2. Windows-VM acceptance tests AT01-AT30. 3. UI Automation audit, sign-in impact measurement, tray monitor, scheduled service ops, remote mode (P2), code signing.

## 1.8
Cleaner expanded (WU cache, DO, shader, dumps, logs, prefetch, fontcache, browsers, DNS, Windows.old), risk column, Empty folders page, Windows 7 compat + x86 build. Roadmap: docs/ROADMAP.md. All 1.7/1.8 additions compile-only.

## 2.0
Navy UI rewrite, deep scan, uninstaller, auto-update, delete options. Compile-checked only. Release package for the public updates repo: releases-repo/.

## 3.0 (WebView2 UI)
New UI = `ui/index.html` (single file, vanilla JS, embedded via `tools/embed.py`) hosted by `src/win/webui.cpp` (WebView2, WebView2Loader.dll loaded at runtime, SDK in `third_party/webview2`, MinGW shim `third_party/shim/EventToken.h`). Bridge = JSON messages (`src/win/json.h`, tested): the page names scan items by key or generation+index, never raw paths. Fallback to classic Win32 UI (main.cpp) when the runtime is missing or `--classic`. Dev preview without Windows: `tools/ui-preview/shot.js` (Playwright + `mock.js`). NOT run on real Windows yet.

## 3.1
UI list framework (ui/index.html: `LS()`, `table()`, `pickRow()`, filters/sort helpers) gives every list checkbox + Ctrl/Shift multi-select, sorting, filters, CSV export. New host commands: drivers.list (WMI) / drivers.check / drivers.install (Windows Update Agent, admin), processes.list/end, sys.info, tools.run (whitelist), export.csv, scan.*, startup.removeDelay. Duplicate groups proven byte-for-byte at scan time; empty-folder tree delete re-verifies then removes deepest-first. UI behaviour tests: `cd tools/ui-preview && NODE_PATH=$(npm root -g) node test.js` (36 checks, mock bridge, Chromium). Still never run inside real WebView2/Windows.

## 3.2 (Phase 1)
New: src/win/quarantine.cpp (restorable quarantine, index.jsonl), Settings fields exclusions/ignored/skipVersion/deferUntil/logRetentionDays/quarantineLeftovers, PreviewClean (dry run), host commands ignore.*, exclusions.*, quarantine.*, recovery.*, clean.preview, update.skip/defer, journal.clear, app.startWithWindows, programs.openFolder. UI: Recovery page, update prompt, previews, ignore, protected-program rules. UI tests: 50 checks (tools/ui-preview/test.js).

## 3.3 (Phase 2)
src/win/phase2.cpp (WMI drivers + PnP problem codes, browser profiles, privacy checklist, event logs via EvtQuery, Redact). webui.cpp: drivers.* (support/verify/enriched check, IDispatch late-binding for IWindowsDriverUpdate because MinGW lacks it), privacy.*, trouble.status, events.query, report.build/save, help.open, files.preview, files.scan mode "downloads". UI: Privacy and Troubleshoot pages, richer Drivers. UI tests: 66 checks.

## 3.4
advice.cpp + data/advice.json (embedded via tools/embed.py as kAdviceJson; same file published in releases-repo/advice.json and fetched by FetchAdviceJson when online). Startup/Service JSON carry adv/advWhy/advSrc. Safe Mode: InSafeMode(), safemode.status/fix (starts Schedule, msiserver, Winmgmt), MSI uninstall starts msiserver. UI: adminGate() before privileged actions, Recommendation column/filter. Files.cpp Join() fixes doubled backslash on drive roots. WebView2 watchdog (25 s) falls back to classic UI.

## 3.5 (Phase 3)
phase3.cpp (org info, perf sample, storage trend, WU registry status, RunCapture, StartupImpact via Diagnostics-Performance event 101, license verify via CNG). webui: dashboard.info, perf.sample, wu.check, startup.impact, store.*, hotfix.*, license.*. UI: richer Today, Programs tabs (Store apps, Windows updates), Boot impact column, license + live-activity settings. Tools: license_keygen/issue, make_sbom, sign.sh/ps1, release.sh. UI tests: 88 checks.

## 3.7 (stabilize, part 1)
device.cpp: DiagnoseDevice (problem code, DEVPKEY_Device_ProblemStatus decoded, driver service state/file, Kernel-PnP 410/411/441/442 events, filters) and FixDevice (rescan / restart / enable / enableService / reinstall, re-checked and journaled). webui: device.diagnose, device.fix, system.restart/cancelRestart; device.fix is on the administrator-helper whitelist. UI: Drivers > Problems tab (what Windows reports, why it cannot start, automatic fix, manual steps, ignore), theme switch (auto/light/dark). UI tests: 102 checks.

## 3.7.1 (stabilize, part 2)
Update: HttpErr text for WinHTTP codes, ApplyUserProxy, long timeouts for installers, retry + jsDelivr mirror, update.openDownload, channel (latest-beta.json), rollback via manifest "previous". What's new (data/whatsnew.json embedded) and Help & FAQ modals. WebView2-missing prompt in main.cpp (Win10/11: offer Microsoft page; Win7/8: explain classic window). clean.runById lets the administrator helper run its own scan and clean selected ids. UI tests: 108 checks.

## 3.7.3 (stabilize, part 3)
log.cpp (app.log, crash filter), selftest.cpp (25+ checks, CLI --selftest), webui ProcessFailed handling, UI: call() timeouts, busy lock, error capture, 1500-row table cap, Settings > Diagnostics. UI tests: 114 checks. The 3.7 stabilize list is complete; remaining stabilization depends on real-PC test results (use the self-test).
