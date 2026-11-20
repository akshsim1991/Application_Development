# Feather System Utilities 2.0 - User guide
One window with Dashboard, Startup, Services, Drivers, Cleaner, Large files, Duplicates, Empty folders, History and Settings. Offline. Developed by Akshaya Simha.

## Layout (2.0)
Left sidebar: **Care** (big SCAN button, Smart or Manual mode), **Custom Clean** (Cleaner, Large files, Duplicates, Empty folders), **Speed Up** (Overview, Startup, Services, Drivers), **Software** (program uninstaller), **Toolbox** (every tool in one place), **Action Center** (history), **Settings**. Ctrl+1..7 switch sections.
**Care**: press SCAN. Smart mode checks junk, registry, startup, services and programs; Manual mode lets you add Large files, Duplicates, Empty folders and Deep scan. A progress ring shows real steps. Nothing is changed by scanning; **Review & Clean** takes you to Custom Clean.
**Custom Clean**: tick items (checkbox column, Space toggles, Select all / Recommended / Select none), then press the green **Clean** button. **Deep scan...** searches all fixed drives for stray .tmp/.temp files, orphaned Office lock files, thumbnail databases, old dumps and logs, plus npm/pip/Yarn/NuGet and Discord/Slack/VS Code/Spotify caches and offers Windows' component-store cleanup (DISM).

## Software uninstaller
Select programs, then **Uninstall** (official uninstaller, long method) or **Uninstall silently** (publisher quiet command, Windows Installer /qn, Inno Setup and NSIS switches; programs without a documented silent switch are skipped). Afterwards, leftovers are searched using evidence: the install folder, folders named after the program in standard locations, vendor registry keys, and startup entries, services, scheduled tasks and Explorer add-ons that point into the removed install folder. They appear in Custom Clean under "Leftovers of uninstalled programs", pre-ticked, and are deleted when you press Clean (folders permanently; registry entries are backed up first; task XML is saved). **Force remove** skips the uninstaller for broken ones. Folders used by another installed program, system and user-document folders are never offered. Store apps and portable apps are not covered yet.

## Updates
Settings > "Check for updates automatically" (default on; once a day) and More > Check for updates. The app downloads latest.json from the project's public releases page on GitHub, asks before downloading, verifies size and SHA-256, installs silently (administrator prompt) and reopens, like Feather PDF. This is the only network use. Turn it off in Settings for a fully offline app.

## Safety model
Reading works with standard rights. Machine-wide changes need "Restart as administrator" (bottom right). Every change is journaled (History page), verified by reading it back, and critical services are blocked from stop/disable/kill. Undo (Ctrl+Z) reverts configuration changes only, never running state.

## Startup
Run/RunOnce (64/32-bit views), Startup folders, scheduled tasks with sign-in/boot triggers, Explorer add-ons. Enable/Disable, Delete (backup kept; Restore...), Add, **Start later** (creates a per-user delayed scheduled task and disables the original; "Remove delay" restores it), multi-select, export.
Windows system items are hidden by default (Settings). "Sign-in impact" is shown as "Not measured": this version does not measure it.

## Services and Drivers
Start/Stop/Restart/Pause/Resume (dependency-aware), Start type (incl. delayed), Kill process (own-process, non-critical, identity-checked), Properties (General, Configuration, Recovery, Dependencies, History) with editors for name/description, recovery and log-on account (built-in accounts only).
**Profiles** are reviewed plans: 5 built-in (privacy, lighter, no Xbox, no printer, security) with stated impact, plus your own profiles and snapshots. Nothing is applied until you press Apply.
Drivers are read-only.

## Cleaner
Scan once, then tick what you want. Every row shows size, file count and a **risk level** (Low: caches/temp; Medium: prefetch, font cache, registry entries; High: Windows.old). Items needing administrator rights say so. Files in use are skipped; files newer than 24 hours are kept (Settings) except for categories where age does not matter (update cache, recent files).
Categories: user and Windows temp, **Windows Update download cache** (Windows Update and BITS are stopped, the cache cleared, and the services restarted; skipped if Windows is installing updates), Delivery Optimization cache, DirectX shader cache, Windows internet cache, thumbnails, error reports, crash dumps and system memory dumps, old Windows logs, recent-files history, prefetch, font cache, Edge/Chrome/Brave/Firefox cache (close the browser first), Recycle Bin, DNS cache flush, and Windows.old (opens Windows' Disk Cleanup; it cannot be deleted safely by hand).
Registry: conservative detection of leftover uninstall entries, broken App Paths, stale MUI cache and shared-DLL references to missing files. Entries are exported first; **Restore registry...** puts them back. Network/removable drives, MSI products and Windows-folder files are never listed.
Temp files are deleted permanently. Cookies, history, passwords and bookmarks are never touched.

## Large files and Duplicates
Both pages scan only when you press **Scan** (default folders: Downloads, Documents, Desktop, Pictures, Videos, Music; change with **Folders...**; set **Minimum size**). They never follow links, never read Windows/Program Files, skip system files, and never download cloud-only placeholders. Large files lists the biggest files (filter by type). Duplicates finds byte-identical files (same size, then SHA-256; hard links are not counted) and groups them; **Auto-select** proposes which copies to remove (keep oldest/newest/shortest path). Removal always goes to the **Recycle Bin**, and at least one copy of every duplicate group is always kept. Stop cancels a scan.

## Empty folders
Finds folders that contain no files at all (hidden/system files and links count as content; Windows, Program Files and system folders are never listed). Scan, review, then Move to Recycle Bin. Details shows how many empty subfolders sit inside.

## Windows version support
Windows 10/11 (x64) are the main targets. The installer also contains an x86 build for 32-bit Windows 7 SP1 and later; on Windows 7/8 some cosmetic features (dark menus, per-monitor DPI) are unavailable. Windows 7 support is built to the Windows 7 API level but has not yet been tested on a real Windows 7 PC.

## Keys
Ctrl+F search, Esc clear, F5 refresh, Enter details, Del delete, Ctrl+A select all, Ctrl+C copy name, Ctrl+Shift+C copy details, Ctrl+E export (CSV/HTML/JSON), Ctrl+Z undo, Ctrl+Shift+S/T/R/K service start/stop/restart/kill, Ctrl+1..7 pages, Ctrl+, settings, F1 about.

## Data
`%LOCALAPPDATA%\FeatherSystemUtilities`: settings.ini, journal.jsonl, backups\, profiles\, firstseen.tsv, legacy\ (copies of old Startup/Services Manager data; originals are never touched or deleted).
Uninstall keeps this folder unless you choose to delete it.

## Known limits
Not deployed or tested on real Windows by the developer yet (see docs/TEST_REPORT.md). No remote-computer mode, boot-time measurement, tray monitor or scheduled service operations in 2.0.
