# Project state
Version 1.5.0 (2026-11-13). Branch `claude/new-session-ann5h4`. Release: `releases/v1.5.0` (setup + portable exe).
## Implemented (Wine-smoke-tested unless noted)
Shell (rail nav, native accessible toolbar buttons, themes x4, settings persistence, dashboard tiles, status bar, single instance, icon), Startup (Run/RunOnce, folders, tasks, add-ons; enable/disable, delete+backup, restore, add, undo, Start later/Remove delay [untested: Task Scheduler], multi-select, filters, hide system items), Services (inventory, start/stop/restart/pause/resume, kill, start type, profiles+snapshots with plan review, Properties + editors + History tab [untested]), Drivers (read-only), History, export CSV/HTML/JSON, legacy data copy, NSIS installer [untested], --cleanup for uninstall.
## Next
1. Owner test on Windows 11; fix issues. 2. Windows-VM acceptance tests AT01-AT30. 3. UI Automation audit, sign-in impact measurement, tray monitor, scheduled service ops, remote mode (P2), code signing.
