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
