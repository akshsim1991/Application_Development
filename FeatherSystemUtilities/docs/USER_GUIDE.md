# Feather System Utilities 1.7 - User guide
One window with Dashboard, Startup, Services, Drivers, Cleaner, Large files, Duplicates, History and Settings. Offline. Developed by Akshaya Simha.

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
Scans temporary-file folders (user and Windows temp, Windows/browser cache for Edge and Chrome, thumbnails, error reports, crash dumps, Recycle Bin) and conservative invalid registry entries (leftover uninstall entries, broken App Paths, stale MUI cache, shared-DLL references to missing files). Select items, then **Clean selected** (always confirmed). Temp files are deleted permanently; files in use and files newer than 24 hours (Settings) are skipped. Registry entries are exported to a backup first and can be put back with **Restore registry...**. Entries on network/removable drives, MSI products and Windows-folder files are never listed. Windows temp and all-users registry items need administrator rights.

## Large files and Duplicates
Both pages scan only when you press **Scan** (default folders: Downloads, Documents, Desktop, Pictures, Videos, Music; change with **Folders...**; set **Minimum size**). They never follow links, never read Windows/Program Files, skip system files, and never download cloud-only placeholders. Large files lists the biggest files (filter by type). Duplicates finds byte-identical files (same size, then SHA-256; hard links are not counted) and groups them; **Auto-select** proposes which copies to remove (keep oldest/newest/shortest path). Removal always goes to the **Recycle Bin**, and at least one copy of every duplicate group is always kept. Stop cancels a scan.

## Keys
Ctrl+F search, Esc clear, F5 refresh, Enter details, Del delete, Ctrl+A select all, Ctrl+C copy name, Ctrl+Shift+C copy details, Ctrl+E export (CSV/HTML/JSON), Ctrl+Z undo, Ctrl+Shift+S/T/R/K service start/stop/restart/kill, Ctrl+1..9 pages, Ctrl+, settings, F1 about.

## Data
`%LOCALAPPDATA%\FeatherSystemUtilities`: settings.ini, journal.jsonl, backups\, profiles\, firstseen.tsv, legacy\ (copies of old Startup/Services Manager data; originals are never touched or deleted).
Uninstall keeps this folder unless you choose to delete it.

## Known limits
Not deployed or tested on real Windows by the developer yet (see docs/TEST_REPORT.md). No remote-computer mode, boot-time measurement, tray monitor or scheduled service operations in 1.7.
