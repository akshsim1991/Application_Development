# Feather System Utilities 1.5 - User guide
One window with Dashboard, Startup, Services, Drivers, History and Settings. Offline. Developed by Akshaya Simha.

## Safety model
Reading works with standard rights. Machine-wide changes need "Restart as administrator" (bottom right). Every change is journaled (History page), verified by reading it back, and critical services are blocked from stop/disable/kill. Undo (Ctrl+Z) reverts configuration changes only, never running state.

## Startup
Run/RunOnce (64/32-bit views), Startup folders, scheduled tasks with sign-in/boot triggers, Explorer add-ons. Enable/Disable, Delete (backup kept; Restore...), Add, **Start later** (creates a per-user delayed scheduled task and disables the original; "Remove delay" restores it), multi-select, export.
Windows system items are hidden by default (Settings). "Sign-in impact" is shown as "Not measured": this version does not measure it.

## Services and Drivers
Start/Stop/Restart/Pause/Resume (dependency-aware), Start type (incl. delayed), Kill process (own-process, non-critical, identity-checked), Properties (General, Configuration, Recovery, Dependencies, History) with editors for name/description, recovery and log-on account (built-in accounts only).
**Profiles** are reviewed plans: 5 built-in (privacy, lighter, no Xbox, no printer, security) with stated impact, plus your own profiles and snapshots. Nothing is applied until you press Apply.
Drivers are read-only.

## Keys
Ctrl+F search, Esc clear, F5 refresh, Enter details, Del delete, Ctrl+A select all, Ctrl+C copy name, Ctrl+Shift+C copy details, Ctrl+E export (CSV/HTML/JSON), Ctrl+Z undo, Ctrl+Shift+S/T/R/K service start/stop/restart/kill, Ctrl+1..6 pages, Ctrl+, settings, F1 about.

## Data
`%LOCALAPPDATA%\FeatherSystemUtilities`: settings.ini, journal.jsonl, backups\, profiles\, firstseen.tsv, legacy\ (copies of old Startup/Services Manager data; originals are never touched or deleted).
Uninstall keeps this folder unless you choose to delete it.

## Known limits
Not deployed or tested on real Windows by the developer yet (see docs/TEST_REPORT.md). No remote-computer mode, boot-time measurement, tray monitor or scheduled service operations in 1.5.
