# Feather System Utilities 1.5.0
Windows 10/11 x64. Offline, no telemetry. Unsigned (SmartScreen may warn).
- `FeatherSystemUtilities-Setup-1.5.0.exe` - offline installer (Program Files, Start menu entry, uninstall entry, keeps your data on uninstall).
- `FeatherSystemUtilities.exe` - portable single exe (same app; data in %LOCALAPPDATA%\FeatherSystemUtilities).
New in 1.5: Dashboard tiles, Drivers page (read-only), real Settings (4 themes, density, hide system items, saved window/columns), Start later (delayed launch) with Remove delay, Profiles/snapshots with review-first plans, Kill process, service name/description/recovery/account editors, service event History tab, export CSV/HTML/JSON, themed dialogs, native accessible toolbar buttons, app icon, installer, legacy-data copy.
Fixed: menu commands from popups (Start later, Profiles, Restore) were ignored; dark header/selection/dialog issues; clipped dialog buttons; search placeholder; stale rail highlight.
Verified only under Wine on Linux with synthetic data (see docs/TEST_REPORT.md). Task Scheduler, event log, installer and real-Windows behaviour are untested.
