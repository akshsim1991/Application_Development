# Feather System Utilities 3.2.0 — Phase 1: safety and trust
- Checks for updates as soon as the app opens (like Feather PDF): Update now / Remind me later / Skip this version.
- Quarantine: leftovers and stray files are moved to a restorable quarantine instead of being deleted. New Recovery page: restore points (list + create, verified), quarantine (restore / delete permanently), backups.
- Dry-run preview of the exact files before cleaning; clear "recoverable / permanent / backed up" labels.
- Exclusions and ignored findings (Clean, Startup, Driver updates), managed in Settings.
- Settings: start with Windows (off by default), keep journal for N days, clear journal.
- Uninstaller safety: security products, drivers and system components cannot be force-removed and get no leftover search; leftovers never include drivers/system folders/system registry. Uninstall command, source and install folder shown.
- Startup: warnings for security / accessibility / device / backup software; Task Manager and Windows Settings links.
- NOT run on real Windows by the developer.
